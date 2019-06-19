#!/usr/bin/python

import optparse
import os
import shlex
import struct
import sys

ARMAG = "!<arch>\n"
SARMAG = 8
ARFMAG = "`\n"
AR_EFMT1 = "#1/"


def memdump(src, bytes_per_line=16, address=0):
    FILTER = ''.join([(len(repr(chr(x))) == 3) and chr(x) or '.'
                     for x in range(256)])
    for i in range(0, len(src), bytes_per_line):
        s = src[i:i+bytes_per_line]
        hex_bytes = ' '.join(["%02x" % (ord(x)) for x in s])
        ascii = s.translate(FILTER)
        print("%#08.8x: %-*s %s" % (address+i, bytes_per_line*3, hex_bytes,
                                    ascii))


class Object(object):
    def __init__(self, file):
        def read_str(file, str_len):
            return file.read(str_len).rstrip('\0 ')

        def read_int(file, str_len, base):
            return int(read_str(file, str_len), base)

        self.offset = file.tell()
        self.file = file
        self.name = read_str(file, 16)
        self.date = read_int(file, 12, 10)
        self.uid = read_int(file, 6, 10)
        self.gid = read_int(file, 6, 10)
        self.mode = read_int(file, 8, 8)
        self.size = read_int(file, 10, 10)
        if file.read(2) != ARFMAG:
            raise ValueError('invalid BSD object at offset %#08.8x' % (
                             self.offset))
        # If we have an extended name read it. Extended names start with
        name_len = 0
        if self.name.startswith(AR_EFMT1):
            name_len = int(self.name[len(AR_EFMT1):], 10)
            self.name = read_str(file, name_len)
        self.obj_offset = file.tell()
        self.obj_size = self.size - name_len
        file.seek(self.obj_size, 1)

    def dump(self, f=sys.stdout, flat=True):
        if flat:
            f.write('%#08.8x: %#08.8x %5u %5u %6o %#08.8x %s\n' % (self.offset,
                    self.date, self.uid, self.gid, self.mode, self.size,
                    self.name))
        else:
            f.write('%#08.8x: \n' % self.offset)
            f.write(' name = "%s"\n' % self.name)
            f.write(' date = %#08.8x\n' % self.date)
            f.write('  uid = %i\n' % self.uid)
            f.write('  gid = %i\n' % self.gid)
            f.write(' mode = %o\n' % self.mode)
            f.write(' size = %#08.8x\n' % (self.size))
            self.file.seek(self.obj_offset, 0)
            first_bytes = self.file.read(4)
            f.write('bytes = ')
            memdump(first_bytes)

    def get_bytes(self):
        saved_pos = self.file.tell()
        self.file.seek(self.obj_offset, 0)
        bytes = self.file.read(self.obj_size)
        self.file.seek(saved_pos, 0)
        return bytes


class StringTable(object):
    def __init__(self, bytes):
        self.bytes = bytes

    def get_string(self, offset):
        length = len(self.bytes)
        if offset >= length:
            return None
        return self.bytes[offset:self.bytes.find('\0', offset)]


class Archive(object):
    def __init__(self, path):
        self.path = path
        self.file = open(path, 'r')
        self.objects = []
        self.offset_to_object = {}
        if self.file.read(SARMAG) != ARMAG:
            print("error: file isn't a BSD archive")
        while True:
            try:
                self.objects.append(Object(self.file))
            except ValueError:
                break

    def get_object_at_offset(self, offset):
        if offset in self.offset_to_object:
            return self.offset_to_object[offset]
        for obj in self.objects:
            if obj.offset == offset:
                self.offset_to_object[offset] = obj
                return obj
        return None

    def find(self, name, mtime=None, f=sys.stdout):
        '''
            Find an object(s) by name with optional modification time. There
            can be multple objects with the same name inside and possibly with
            the same modification time within a BSD archive so clients must be
            prepared to get multiple results.
        '''
        matches = []
        for obj in self.objects:
            if obj.name == name and (mtime is None or mtime == obj.date):
                matches.append(obj)
        return matches

    @classmethod
    def dump_header(self, f=sys.stdout):
        f.write('            DATE       UID   GID   MODE   SIZE       NAME\n')
        f.write('            ---------- ----- ----- ------ ---------- '
                '--------------\n')

    def get_symdef(self):
        def get_uint32(file):
            '''Extract a uint32_t from the current file position.'''
            v, = struct.unpack('=I', file.read(4))
            return v

        for obj in self.objects:
            symdef = []
            if obj.name.startswith("__.SYMDEF"):
                self.file.seek(obj.obj_offset, 0)
                ranlib_byte_size = get_uint32(self.file)
                num_ranlib_structs = ranlib_byte_size/8
                str_offset_pairs = []
                for _ in range(num_ranlib_structs):
                    strx = get_uint32(self.file)
                    offset = get_uint32(self.file)
                    str_offset_pairs.append((strx, offset))
                strtab_len = get_uint32(self.file)
                strtab = StringTable(self.file.read(strtab_len))
                for s in str_offset_pairs:
                    symdef.append((strtab.get_string(s[0]), s[1]))
            return symdef

    def get_object_dicts(self):
        '''
            Returns an array of object dictionaries that contain they following
            keys:
                'object': the actual bsd.Object instance
                'symdefs': an array of symbol names that the object contains
                           as found in the "__.SYMDEF" item in the archive
        '''
        symdefs = self.get_symdef()
        symdef_dict = {}
        if symdefs:
            for (name, offset) in symdefs:
                if offset in symdef_dict:
                    object_dict = symdef_dict[offset]
                else:
                    object_dict = {
                        'object': self.get_object_at_offset(offset),
                        'symdefs': []
                    }
                    symdef_dict[offset] = object_dict
                object_dict['symdefs'].append(name)
        object_dicts = []
        for offset in sorted(symdef_dict):
            object_dicts.append(symdef_dict[offset])
        return object_dicts

    def dump(self, f=sys.stdout, flat=True):
        f.write('%s:\n' % self.path)
        if flat:
            self.dump_header(f=f)
        for obj in self.objects:
            obj.dump(f=f, flat=flat)


def main():
    parser = optparse.OptionParser(
        prog='bsd',
        description='Utility for BSD archives')
    parser.add_option(
        '--object',
        type='string',
        dest='object_name',
        default=None,
        help=('Specify the name of a object within the BSD archive to get '
              'information on'))
    parser.add_option(
        '-s', '--symbol',
        type='string',
        dest='find_symbol',
        default=None,
        help=('Specify the name of a symbol within the BSD archive to get '
              'information on from SYMDEF'))
    parser.add_option(
        '--symdef',
        action='store_true',
        dest='symdef',
        default=False,
        help=('Dump the information in the SYMDEF.'))
    parser.add_option(
        '-v', '--verbose',
        action='store_true',
        dest='verbose',
        default=False,
        help='Enable verbose output')
    parser.add_option(
        '-e', '--extract',
        action='store_true',
        dest='extract',
        default=False,
        help=('Specify this to extract the object specified with the --object '
              'option. There must be only one object with a matching name or '
              'the --mtime option must be specified to uniquely identify a '
              'single object.'))
    parser.add_option(
        '-m', '--mtime',
        type='int',
        dest='mtime',
        default=None,
        help=('Specify the modification time of the object an object. This '
              'option is used with either the --object or --extract options.'))
    parser.add_option(
        '-o', '--outfile',
        type='string',
        dest='outfile',
        default=None,
        help=('Specify a different name or path for the file to extract when '
              'using the --extract option. If this option isn\'t specified, '
              'then the extracted object file will be extracted into the '
              'current working directory if a file doesn\'t already exist '
              'with that name.'))

    (options, args) = parser.parse_args(sys.argv[1:])

    for path in args:
        archive = Archive(path)
        if options.object_name:
            print('%s:\n' % (path))
            matches = archive.find(options.object_name, options.mtime)
            if matches:
                dump_all = True
                if options.extract:
                    if len(matches) == 1:
                        dump_all = False
                        if options.outfile is None:
                            outfile_path = matches[0].name
                        else:
                            outfile_path = options.outfile
                        if os.path.exists(outfile_path):
                            print('error: outfile "%s" already exists' % (
                              outfile_path))
                        else:
                            print('Saving file to "%s"...' % (outfile_path))
                            with open(outfile_path, 'w') as outfile:
                                outfile.write(matches[0].get_bytes())
                    else:
                        print('error: multiple objects match "%s". Specify '
                              'the modification time using --mtime.' % (
                                options.object_name))
                if dump_all:
                    for obj in matches:
                        obj.dump(flat=False)
            else:
                print('error: object "%s" not found in archive' % (
                      options.object_name))
        elif options.find_symbol:
            symdefs = archive.get_symdef()
            if symdefs:
                success = False
                for (name, offset) in symdefs:
                    obj = archive.get_object_at_offset(offset)
                    if name == options.find_symbol:
                        print('Found "%s" in:' % (options.find_symbol))
                        obj.dump(flat=False)
                        success = True
                if not success:
                    print('Didn\'t find "%s" in any objects' % (
                          options.find_symbol))
            else:
                print("error: no __.SYMDEF was found")
        elif options.symdef:
            object_dicts = archive.get_object_dicts()
            for object_dict in object_dicts:
                object_dict['object'].dump(flat=False)
                print("symbols:")
                for name in object_dict['symdefs']:
                    print("  %s" % (name))
        else:
            archive.dump(flat=not options.verbose)


if __name__ == '__main__':
    main()


def print_mtime_error(result, dmap_mtime, actual_mtime):
    print >>result, ("error: modification time in debug map (%#08.8x) doesn't "
                     "match the .o file modification time (%#08.8x)" % (
                        dmap_mtime, actual_mtime))


def print_file_missing_error(result, path):
    print >>result, "error: file \"%s\" doesn't exist" % (path)


def print_multiple_object_matches(result, object_name, mtime, matches):
    print >>result, ("error: multiple matches for object '%s' with with "
                     "modification time %#08.8x:" % (object_name, mtime))
    Archive.dump_header(f=result)
    for match in matches:
        match.dump(f=result, flat=True)


def print_archive_object_error(result, object_name, mtime, archive):
    matches = archive.find(object_name, f=result)
    if len(matches) > 0:
        print >>result, ("error: no objects have a modification time that "
                         "matches %#08.8x for '%s'. Potential matches:" % (
                            mtime, object_name))
        Archive.dump_header(f=result)
        for match in matches:
            match.dump(f=result, flat=True)
    else:
        print >>result, "error: no object named \"%s\" found in archive:" % (
            object_name)
        Archive.dump_header(f=result)
        for match in archive.objects:
            match.dump(f=result, flat=True)
        # archive.dump(f=result, flat=True)


class VerifyDebugMapCommand:
    name = "verify-debug-map-objects"

    def create_options(self):
        usage = "usage: %prog [options]"
        description = '''This command reports any .o files that are missing
or whose modification times don't match in the debug map of an executable.'''

        self.parser = optparse.OptionParser(
            description=description,
            prog=self.name,
            usage=usage,
            add_help_option=False)

        self.parser.add_option(
            '-e', '--errors',
            action='store_true',
            dest='errors',
            default=False,
            help="Only show errors")

    def get_short_help(self):
        return "Verify debug map object files."

    def get_long_help(self):
        return self.help_string

    def __init__(self, debugger, unused):
        self.create_options()
        self.help_string = self.parser.format_help()

    def __call__(self, debugger, command, exe_ctx, result):
        import lldb
        # Use the Shell Lexer to properly parse up command options just like a
        # shell would
        command_args = shlex.split(command)

        try:
            (options, args) = self.parser.parse_args(command_args)
        except:
            result.SetError("option parsing failed")
            return

        # Always get program state from the SBExecutionContext passed in
        target = exe_ctx.GetTarget()
        if not target.IsValid():
            result.SetError("invalid target")
            return
        archives = {}
        for module_spec in args:
            module = target.module[module_spec]
            if not (module and module.IsValid()):
                result.SetError('error: invalid module specification: "%s". '
                                'Specify the full path, basename, or UUID of '
                                'a module ' % (module_spec))
                return
            num_symbols = module.GetNumSymbols()
            num_errors = 0
            for i in range(num_symbols):
                symbol = module.GetSymbolAtIndex(i)
                if symbol.GetType() != lldb.eSymbolTypeObjectFile:
                    continue
                path = symbol.GetName()
                if not path:
                    continue
                # Extract the value of the symbol by dumping the
                # symbol. The value is the mod time.
                dmap_mtime = int(str(symbol).split('value = ')
                                 [1].split(',')[0], 16)
                if not options.errors:
                    print >>result, '%s' % (path)
                if os.path.exists(path):
                    actual_mtime = int(os.stat(path).st_mtime)
                    if dmap_mtime != actual_mtime:
                        num_errors += 1
                        if options.errors:
                            print >>result, '%s' % (path),
                        print_mtime_error(result, dmap_mtime,
                                          actual_mtime)
                elif path[-1] == ')':
                    (archive_path, object_name) = path[0:-1].split('(')
                    if not archive_path and not object_name:
                        num_errors += 1
                        if options.errors:
                            print >>result, '%s' % (path),
                        print_file_missing_error(path)
                        continue
                    if not os.path.exists(archive_path):
                        num_errors += 1
                        if options.errors:
                            print >>result, '%s' % (path),
                        print_file_missing_error(archive_path)
                        continue
                    if archive_path in archives:
                        archive = archives[archive_path]
                    else:
                        archive = Archive(archive_path)
                        archives[archive_path] = archive
                    matches = archive.find(object_name, dmap_mtime)
                    num_matches = len(matches)
                    if num_matches == 1:
                        print >>result, '1 match'
                        obj = matches[0]
                        if obj.date != dmap_mtime:
                            num_errors += 1
                            if options.errors:
                                print >>result, '%s' % (path),
                            print_mtime_error(result, dmap_mtime, obj.date)
                    elif num_matches == 0:
                        num_errors += 1
                        if options.errors:
                            print >>result, '%s' % (path),
                        print_archive_object_error(result, object_name,
                                                   dmap_mtime, archive)
                    elif num_matches > 1:
                        num_errors += 1
                        if options.errors:
                            print >>result, '%s' % (path),
                        print_multiple_object_matches(result,
                                                      object_name,
                                                      dmap_mtime, matches)
            if num_errors > 0:
                print >>result, "%u errors found" % (num_errors)
            else:
                print >>result, "No errors detected in debug map"


def __lldb_init_module(debugger, dict):
    # This initializer is being run from LLDB in the embedded command
    # interpreter.
    # Add any commands contained in this module to LLDB
    debugger.HandleCommand(
        'command script add -c %s.VerifyDebugMapCommand %s' % (
            __name__, VerifyDebugMapCommand.name))
    print('The "%s" command has been installed, type "help %s" for detailed '
          'help.' % (VerifyDebugMapCommand.name, VerifyDebugMapCommand.name))
