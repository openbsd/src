/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _DEVICE_C_
#define _DEVICE_C_

#include <stdio.h>

#include "device_table.h"
#include "cap.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <ctype.h>

STATIC_INLINE_DEVICE (void) clean_device_properties(device *);
STATIC_INLINE_DEVICE (void) init_device_properties(device *, void*);

/* property entries */

typedef struct _device_property_entry device_property_entry;
struct _device_property_entry {
  device_property_entry *next;
  device_property *value;
  const void *init_array;
  unsigned sizeof_init_array;
};


/* Interrupt edges */

typedef struct _device_interrupt_edge device_interrupt_edge;
struct _device_interrupt_edge {
  int my_port;
  device *dest;
  int dest_port;
  device_interrupt_edge *next;
  object_disposition disposition;
};

STATIC_INLINE_DEVICE\
(void)
attach_device_interrupt_edge(device_interrupt_edge **list,
			     int my_port,
			     device *dest,
			     int dest_port,
			     object_disposition disposition)
{
  device_interrupt_edge *new_edge = ZALLOC(device_interrupt_edge);
  new_edge->my_port = my_port;
  new_edge->dest = dest;
  new_edge->dest_port = dest_port;
  new_edge->next = *list;
  new_edge->disposition = disposition;
  *list = new_edge;
}

STATIC_INLINE_DEVICE\
(void)
detach_device_interrupt_edge(device *me,
			     device_interrupt_edge **list,
			     int my_port,
			     device *dest,
			     int dest_port)
{
  while (*list != NULL) {
    device_interrupt_edge *old_edge = *list;
    if (old_edge->dest == dest
	&& old_edge->dest_port == dest_port
	&& old_edge->my_port == my_port) {
      if (old_edge->disposition == permenant_object)
	device_error(me, "attempt to delete permenant interrupt\n");
      *list = old_edge->next;
      zfree(old_edge);
      return;
    }
  }
  device_error(me, "attempt to delete unattached interrupt\n");
}

STATIC_INLINE_DEVICE\
(void)
clean_device_interrupt_edges(device_interrupt_edge **list)
{
  while (*list != NULL) {
    device_interrupt_edge *old_edge = *list;
    switch (old_edge->disposition) {
    case permenant_object:
      list = &old_edge->next;
      break;
    case tempoary_object:
      *list = old_edge->next;
      zfree(old_edge);
      break;
    }
  }
}


/* A device */

struct _device {

  /* my name is ... */
  const char *name;
  device_unit unit_address;
  const char *path;

  /* device tree */
  device *parent;
  device *children;
  device *sibling;

  /* its template methods */
  void *data; /* device specific data */
  const device_callbacks *callback;

  /* device properties */
  device_property_entry *properties;

  /* interrupts */
  device_interrupt_edge *interrupt_destinations;

  /* any open instances of this device */
  device_instance *instances;

  /* the internal/external mappings and other global requirements */
  cap *ihandles;
  cap *phandles;
  psim *system;
};


/* an instance of a device */
struct _device_instance {
  void *data;
  char *args;
  char *path;
  const device_instance_callbacks *callback;
  /* the root instance */
  device *owner;
  device_instance *next;
  /* interposed instance */
  device_instance *parent;
  device_instance *child;
};



/* Device node: */

INLINE_DEVICE\
(device *)
device_parent(device *me)
{
  return me->parent;
}

INLINE_DEVICE\
(device *)
device_sibling(device *me)
{
  return me->sibling;
}

INLINE_DEVICE\
(device *)
device_child(device *me)
{
  return me->children;
}

INLINE_DEVICE\
(const char *)
device_name(device *me)
{
  return me->name;
}

INLINE_DEVICE\
(const char *)
device_path(device *me)
{
  return me->path;
}

INLINE_DEVICE\
(void *)
device_data(device *me)
{
  return me->data;
}

INLINE_DEVICE\
(psim *)
device_system(device *me)
{
  return me->system;
}

INLINE_DEVICE\
(const device_unit *)
device_unit_address(device *me)
{
  return &me->unit_address;
}

INLINE_DEVICE\
(void)
device_address_to_attach_address(device *me,
				 const device_unit *address,
				 int *attach_space,
				 unsigned_word *attach_address)
{
  if (me->callback->convert.address_to_attach_address == NULL)
    device_error(me, "no convert.address_to_attach_address method\n");
  me->callback->convert.address_to_attach_address(me, address, attach_space, attach_address);
}

INLINE_DEVICE\
(void)
device_size_to_attach_size(device *me,
			   const device_unit *size,
			   unsigned *nr_bytes)
{
  if (me->callback->convert.size_to_attach_size == NULL)
    device_error(me, "no convert.size_to_attach_size method\n");
  me->callback->convert.size_to_attach_size(me, size, nr_bytes);
}

STATIC_INLINE_DEVICE\
(int)
device_decode_unit(device *me,
		   const char *unit,
		   device_unit *address)
{
  if (me->callback->convert.decode_unit == NULL)
    device_error(me, "no convert.decode_unit method\n");
  return me->callback->convert.decode_unit(me, unit, address);
}

STATIC_INLINE_DEVICE\
(int)
device_encode_unit(device *me,
		   const device_unit *unit_address,
		   char *buf,
		   int sizeof_buf)
{
  if (me->callback->convert.encode_unit == NULL)
    device_error(me, "no convert.encode_unit method\n");
  return me->callback->convert.encode_unit(me, unit_address, buf, sizeof_buf);
}


/* device template: */

/* determine the full name of the device.  If buf is specified it is
   stored in there.  Failing that, a safe area of memory is allocated */

STATIC_INLINE_DEVICE\
(const char *)
device_full_name(device *leaf,
		 char *buf,
		 unsigned sizeof_buf)
{
  /* get a buffer */
  char full_name[1024];
  if (buf == (char*)0) {
    buf = full_name;
    sizeof_buf = sizeof(full_name);
  }

  /* construct a name */
  if (leaf->parent == NULL) {
    if (sizeof_buf < 1)
      error("device_full_name: buffer overflow\n");
    *buf = '\0';
  }
  else {
    char unit[1024];
    device_full_name(leaf->parent, buf, sizeof_buf);
    if (leaf->parent != NULL
	&& device_encode_unit(leaf->parent,
			      &leaf->unit_address,
			      unit+1,
			      sizeof(unit)-1) > 0)
      unit[0] = '@';
    else
      unit[0] = '\0';
    if (strlen(buf) + strlen("/") + strlen(leaf->name) + strlen(unit)
	>= sizeof_buf)
      error("device_full_name: buffer overflow\n");
    strcat(buf, "/");
    strcat(buf, leaf->name);
    strcat (buf, unit);
  }
  
  /* return it usefully */
  if (buf == full_name)
    buf = (char *) strdup(full_name);
  return buf;
}

/* manipulate/lookup device names */

typedef struct _name_specifier {
  /* components in the full length name */
  char *path;
  char *property;
  char *value;
  /* current device */
  char *name;
  char *unit;
  char *args;
  /* previous device */
  char *last_name;
  char *last_unit;
  char *last_args;
  /* work area */
  char buf[1024];
} name_specifier;

/* Given a device specifier, break it up into its main components:
   path (and if present) property name and property value. */
STATIC_INLINE_DEVICE\
(int)
split_device_specifier(const char *device_specifier,
		       name_specifier *spec)
{
  char *chp;
  if (strlen(device_specifier) >= sizeof(spec->buf))
    error("split_device_specifier: buffer overflow\n");

  /* expand aliases (later) */
  strcpy(spec->buf, device_specifier);

  /* strip the leading spaces and check that remainder isn't a comment */
  chp = spec->buf;
  while (*chp != '\0' && isspace(*chp))
    chp++;
  if (*chp == '\0' || *chp == '#')
    return 0;

  /* find the path and terminate it with null */
  spec->path = chp;
  while (*chp != '\0' && !isspace(*chp))
    chp++;
  if (*chp != '\0') {
    *chp = '\0';
    chp++;
  }

  /* and any value */
  while (*chp != '\0' && isspace(*chp))
    chp++;
  spec->value = chp;

  /* now go back and chop the property off of the path */
  if (spec->value[0] == '\0') {
    spec->property = NULL; /*not a property*/
    spec->value = NULL;
  }
  else if (spec->value[0] == '>'
	   || spec->value[0] == '<') {
    /* an interrupt spec */
    spec->property = NULL;
  }
  else {
    chp = strrchr(spec->path, '/');
    if (chp == NULL) {
      spec->property = spec->path;
      spec->path = strchr(spec->property, '\0');
    }
    else {
      *chp = '\0';
      spec->property = chp+1;
    }
  }

  /* and mark the rest as invalid */
  spec->name = NULL;
  spec->unit = NULL;
  spec->args = NULL;
  spec->last_name = NULL;
  spec->last_unit = NULL;
  spec->last_args = NULL;

  return 1;
}

/* given a device specifier break it up into its main components -
   path and property name - assuming that the last `device' is a
   property name. */
STATIC_INLINE_DEVICE\
(int)
split_property_specifier(const char *property_specifier,
			 name_specifier *spec)
{
  if (split_device_specifier(property_specifier, spec)) {
    if (spec->property == NULL) {
      /* force the last name to be a property name */
      char *chp = strrchr(spec->path, '/');
      if (chp == NULL) {
	spec->property = spec->path;
	spec->path = strrchr(spec->property, '\0');;
      }
      else {
	*chp = '\0';
	spec->property = chp+1;
      }
    }
    return 1;
  }
  else
    return 0;
}

/* parse the next device name and split it up, return 0 when no more
   names to parse */
STATIC_INLINE_DEVICE\
(int)
split_device_name(name_specifier *spec)
{
  char *chp;
  /* remember what came before */
  spec->last_name = spec->name;
  spec->last_unit = spec->unit;
  spec->last_args = spec->args;
  /* finished? */
  if (spec->path[0] == '\0') {
    spec->name = NULL;
    spec->unit = NULL;
    spec->args = NULL;
    return 0;
  }
  /* break the device name from the path */
  spec->name = spec->path;
  chp = strchr(spec->name, '/');
  if (chp == NULL)
    spec->path = strchr(spec->name, '\0');
  else {
    spec->path = chp+1;
    *chp = '\0';
  }
  /* now break out the unit */
  chp = strchr(spec->name, '@');
  if (chp == NULL) {
    spec->unit = NULL;
    chp = spec->name;
  }
  else {
    *chp = '\0';
    chp += 1;
    spec->unit = chp;
  }
  /* finally any args */
  chp = strchr(chp, ':');
  if (chp == NULL)
    spec->args = NULL;
  else {
    *chp = '\0';
    spec->args = chp+1;
  }
  return 1;
}

/* parse the value, returning the next non-space token */

STATIC_INLINE_DEVICE\
(char *)
split_value(name_specifier *spec)
{
  char *token;
  if (spec->value == NULL)
    return NULL;
  /* skip leading white space */
  while (isspace(spec->value[0]))
    spec->value++;
  if (spec->value[0] == '\0') {
    spec->value = NULL;
    return NULL;
  }
  token = spec->value;
  /* find trailing space */
  while (spec->value[0] != '\0' && !isspace(spec->value[0]))
    spec->value++;
  /* chop this value out */
  if (spec->value[0] != '\0') {
    spec->value[0] = '\0';
    spec->value++;
  }
  return token;
}



/* traverse the path specified by spec starting at current */

STATIC_INLINE_DEVICE\
(device *)
split_find_device(device *current,
		  name_specifier *spec)
{
  /* strip off (and process) any leading ., .., ./ and / */
  while (1) {
    if (strncmp(spec->path, "/", strlen("/")) == 0) {
      /* cd /... */
      while (current != NULL && current->parent != NULL)
	current = current->parent;
      spec->path += strlen("/");
    }
    else if (strncmp(spec->path, "./", strlen("./")) == 0) {
      /* cd ./... */
      current = current;
      spec->path += strlen("./");
    }
    else if (strncmp(spec->path, "../", strlen("../")) == 0) {
      /* cd ../... */
      if (current != NULL && current->parent != NULL)
	current = current->parent;
      spec->path += strlen("../");
    }
    else if (strcmp(spec->path, ".") == 0) {
      /* cd . */
      current = current;
      spec->path += strlen(".");
    }
    else if (strcmp(spec->path, "..") == 0) {
      /* cd . */
      if (current != NULL && current->parent != NULL)
	current = current->parent;
      spec->path += strlen("..");
    }
    else
      break;
  }

  /* now go through the path proper */

  if (current == NULL) {
    split_device_name(spec);
    return current;
  }

  while (split_device_name(spec)) {
    device_unit phys;
    device *child;
    device_decode_unit(current, spec->unit, &phys);
    for (child = current->children; child != NULL; child = child->sibling) {
      if (strcmp(spec->name, child->name) == 0) {
	if (phys.nr_cells == 0
	    || memcmp(&phys, &child->unit_address, sizeof(device_unit)) == 0)
	  break;
      }
    }
    if (child == NULL)
      return current; /* search failed */
    current = child;
  }

  return current;
}


STATIC_INLINE_DEVICE\
(device *)
device_create_from(const char *name,
		   const device_unit *unit_address,
		   void *data,
		   const device_callbacks *callbacks,
		   device *parent)
{
  device *new_device = ZALLOC(device);

  /* insert it into the device tree */
  new_device->parent = parent;
  new_device->children = NULL;
  if (parent != NULL) {
    device **sibling = &parent->children;
    while ((*sibling) != NULL)
      sibling = &(*sibling)->sibling;
    *sibling = new_device;
  }

  /* give it a name */
  new_device->name = (char *) strdup(name);
  new_device->unit_address = *unit_address;
  new_device->path = device_full_name(new_device, NULL, 0);

  /* its template */
  new_device->data = data;
  new_device->callback = callbacks;

  /* its properties - already null */
  /* interrupts - already null */

  /* mappings - if needed */
  if (parent == NULL) {
    new_device->ihandles = cap_create(name);
    new_device->phandles = cap_create(name);
  }

  return new_device;
}


STATIC_INLINE_DEVICE\
(device *)
device_template_create_device(device *parent,
			      const char *name,
			      const char *unit_address,
			      const char *args)
{
  const device_descriptor *const *table;
  int name_len;
  char *chp;
  chp = strchr(name, '@');
  name_len = (chp == NULL ? strlen(name) : chp - name);
  for (table = device_table; *table != NULL; table++) {
    const device_descriptor *descr;
    for (descr = *table; descr->name != NULL; descr++) {
      if (strncmp(name, descr->name, name_len) == 0
	  && (descr->name[name_len] == '\0'
	      || descr->name[name_len] == '@')) {
	device_unit address = { 0 };
	void *data = NULL;
	if (parent != NULL && parent->callback->convert.decode_unit != NULL)
	  device_decode_unit(parent, 
			     unit_address,
			     &address);
	if (descr->creator != NULL)
	  data = descr->creator(name, &address, args);
	return device_create_from(name, &address, data,
				  descr->callbacks, parent);
      }
    }
  }
  device_error(parent, "attempt to attach unknown device %s\n", name);
  return NULL;
}


/* device-instance: */

INLINE_DEVICE\
(device_instance *)
device_create_instance_from(device *me,
			    device_instance *parent,
			    void *data,
			    const char *path,
			    const char *args,
			    const device_instance_callbacks *callbacks)
{
  device_instance *instance = ZALLOC(device_instance);
  if ((me == NULL) == (parent == NULL))
    device_error(me, "can't have both parent instance and parent device\n");
  instance->owner = me;
  instance->parent = parent;
  instance->data = data;
  instance->args = (args == NULL ? NULL : (char *) strdup(args));
  instance->path = (path == NULL ? NULL : (char *) strdup(path));
  instance->callback = callbacks;
  /*instance->unit*/
  if (me != NULL) {
    instance->next = me->instances;
    me->instances = instance;
  }
  if (parent != NULL) {
    device_instance **previous;
    parent->child = instance;
    instance->owner = parent->owner;
    instance->next = parent->next;
    /* replace parent with this new node */
    previous = &instance->owner->instances;
    while (*previous != parent) {
      ASSERT(*previous != NULL);
      previous = &(*previous)->next;
    }
    *previous = instance;
  }
  return instance;
}


INLINE_DEVICE\
(device_instance *)
device_create_instance(device *me,
		       const char *device_specifier)
{
  /* find the device node */
  name_specifier spec;
  if (!split_device_specifier(device_specifier, &spec))
    return NULL;
  me = split_find_device(me, &spec);
  if (spec.name != NULL)
    return NULL;
  /* create the instance */
  if (me->callback->instance_create == NULL)
    device_error(me, "no instance_create method\n");
  return me->callback->instance_create(me,
				       device_specifier, spec.last_args);
}

STATIC_INLINE_DEVICE\
(void)
clean_device_instances(device *me)
{
  device_instance **instance = &me->instances;
  while (*instance != NULL) {
    device_instance *old_instance = *instance;
    device_instance_delete(old_instance);
    instance = &me->instances;
  }
}

INLINE_DEVICE\
(void)
device_instance_delete(device_instance *instance)
{
  device *me = instance->owner;
  if (instance->callback->delete == NULL)
    device_error(me, "no delete method\n");
  instance->callback->delete(instance);
  if (instance->args != NULL)
    zfree(instance->args);
  if (instance->path != NULL)
    zfree(instance->path);
  if (instance->child == NULL) {
    /* only remove leaf nodes */
    device_instance **curr = &me->instances;
    while (*curr != instance) {
      ASSERT(*curr != NULL);
      curr = &(*curr)->next;
    }
    *curr = instance->next;
  }
  else {
    /* check it isn't in the instance list */
    device_instance *curr = me->instances;
    while (curr != NULL) {
      ASSERT(curr != instance);
      curr = curr->next;
    }
    /* unlink the child */
    ASSERT(instance->child->parent == instance);
    instance->child->parent = NULL;
  }
  zfree(instance);
}

INLINE_DEVICE\
(int)
device_instance_read(device_instance *instance,
		     void *addr,
		     unsigned_word len)
{
  device *me = instance->owner;
  if (instance->callback->read == NULL)
    device_error(me, "no read method\n");
  return instance->callback->read(instance, addr, len);
}

INLINE_DEVICE\
(int)
device_instance_write(device_instance *instance,
		      const void *addr,
		      unsigned_word len)
{
  device *me = instance->owner;
  if (instance->callback->write == NULL)
    device_error(me, "no write method\n");
  return instance->callback->write(instance, addr, len);
}

INLINE_DEVICE\
(int)
device_instance_seek(device_instance *instance,
		     unsigned_word pos_hi,
		     unsigned_word pos_lo)
{
  device *me = instance->owner;
  if (instance->callback->seek == NULL)
    device_error(me, "no seek method\n");
  return instance->callback->seek(instance, pos_hi, pos_lo);
}

INLINE_DEVICE\
(unsigned_cell)
device_instance_call_method(device_instance *instance,
			    const char *method_name,
			    int n_stack_args,
			    unsigned_cell stack_args[/*n_stack_args*/],	
			    int n_stack_returns,
			    unsigned_cell stack_returns[/*n_stack_args*/])
{
  device *me = instance->owner;
  const device_instance_methods *method = instance->callback->methods;
  if (method == NULL) {
    device_error(me, "no methods (want %s)\n", method_name);
  }
  while (method->name != NULL) {
    if (strcmp(method->name, method_name) == 0) {
      return method->method(instance,
			    n_stack_args, stack_args,
			    n_stack_returns, stack_returns);
    }
  }
  device_error(me, "no %s method\n", method_name);
  return 0;
}


INLINE_DEVICE\
(device *)
device_instance_device(device_instance *instance)
{
  return instance->owner;
}

INLINE_DEVICE\
(const char *)
device_instance_path(device_instance *instance)
{
  return instance->path;
}

INLINE_DEVICE\
(void *)
device_instance_data(device_instance *instance)
{
  return instance->data;
}



/* Device initialization: */

STATIC_INLINE_DEVICE\
(void)
clean_device(device *root,
	     void *data)
{
  psim *system;
  system = (psim*)data;
  clean_device_interrupt_edges(&root->interrupt_destinations);
  clean_device_instances(root);
  clean_device_properties(root);
}

STATIC_INLINE_DEVICE\
(void)
init_device_address(device *me,
		    void *data)
{
  psim *system = (psim*)data;
  TRACE(trace_device_init, ("init_device_address() initializing %s\n", me->path));
  me->system = system; /* misc things not known until now */
  if (me->callback->init.address != NULL)
    me->callback->init.address(me);
}

STATIC_INLINE_DEVICE\
(void)
init_device_data(device *me,
		 void *data)
{
  TRACE(trace_device_init, ("device_init_data() initializing %s\n", me->path));
  if (me->callback->init.data != NULL)
    me->callback->init.data(me);
}

INLINE_DEVICE\
(void)
device_tree_init(device *root,
		 psim *system)
{
  TRACE(trace_device_tree, ("device_tree_init(root=0x%lx, system=0x%lx)\n",
			    (long)root,
			    (long)system));
  /* remove the old, rebuild the new */
  device_tree_traverse(root, clean_device, NULL, system);
  TRACE(trace_tbd, ("Need to dump the device tree here\n"));
  device_tree_traverse(root, init_device_address, NULL, system);
  device_tree_traverse(root, init_device_properties, NULL, system);
  device_tree_traverse(root, init_device_data, NULL, system);
  TRACE(trace_device_tree, ("device_tree_init() = void\n"));
}



/* Device Properties: */

/* local - not available externally */
STATIC_INLINE_DEVICE\
(void)
device_add_property(device *me,
		    const char *property,
		    device_property_type type,
		    const void *init_array,
		    unsigned sizeof_init_array,
		    const void *array,
		    unsigned sizeof_array,
		    const device_property *original,
		    object_disposition disposition)
{
  device_property_entry *new_entry = NULL;
  device_property *new_value = NULL;
  void *new_array = NULL;
  void *new_init_array = NULL;

  /* find the list end */
  device_property_entry **insertion_point = &me->properties;
  while (*insertion_point != NULL) {
    if (strcmp((*insertion_point)->value->name, property) == 0)
      return;
    insertion_point = &(*insertion_point)->next;
  }

  /* create a new value */
  new_value = ZALLOC(device_property);
  new_value->name = (char *) strdup(property);
  new_value->type = type;
  new_value->sizeof_array = sizeof_array;
  new_array = (sizeof_array > 0 ? zalloc(sizeof_array) : NULL);
  new_value->array = new_array;
  new_value->owner = me;
  new_value->original = original;
  new_value->disposition = disposition;
  if (sizeof_array > 0)
    memcpy(new_array, array, sizeof_array);

  /* insert the value into the list */
  new_entry = ZALLOC(device_property_entry);
  *insertion_point = new_entry;
  new_entry->sizeof_init_array = sizeof_init_array;
  new_init_array = (sizeof_init_array > 0 ? zalloc(sizeof_init_array) : NULL);
  new_entry->init_array = new_init_array;
  new_entry->value = new_value;
  if (sizeof_init_array > 0)
    memcpy(new_init_array, init_array, sizeof_init_array);

}


/* local - not available externally */
STATIC_INLINE_DEVICE\
(void)
device_set_property(device *me,
		    const char *property,
		    device_property_type type,
		    const void *array,
		    int sizeof_array,
		    const device_property *original)
{
  /* find the property */
  device_property_entry *entry = me->properties;
  while (entry != NULL) {
    if (strcmp(entry->value->name, property) == 0) {
      void *new_array = 0;
      device_property *value = entry->value;
      /* check the type matches */
      if (value->type != type)
	device_error(me, "conflict between type of new and old value for property %s\n", property);
      /* replace its value */
      if (value->array != NULL)
	zfree((void*)value->array);
      new_array = (sizeof_array > 0
		   ? zalloc(sizeof_array)
		   : (void*)0);
      value->array = new_array;
      value->sizeof_array = sizeof_array;
      if (sizeof_array > 0)
	memcpy(new_array, array, sizeof_array);
      return;
    }
    entry = entry->next;
  }
  device_add_property(me, property, type,
		      NULL, 0, array, sizeof_array,
		      original, tempoary_object);
}


STATIC_INLINE_DEVICE\
(void)
clean_device_properties(device *me)
{
  device_property_entry **delete_point = &me->properties;
  while (*delete_point != NULL) {
    device_property_entry *current = *delete_point;
    device_property *property = current->value;
    switch (current->value->disposition) {
    case permenant_object:
      {
	/* delete the property, and replace it with the original */
	ASSERT(((property->array == NULL) == (current->init_array == NULL))
	       || property->type == ihandle_property);
	if (current->init_array != NULL) {
	  zfree((void*)current->value->array);
	  current->value->array = NULL;
	  if (property->type != ihandle_property) {
	    device_set_property(me, property->name,
				property->type,
				current->init_array, current->sizeof_init_array,
				NULL);
	  }
	}
	delete_point = &(*delete_point)->next;
      }
      break;
    case tempoary_object:
      {
	/* zap the actual property, was created during simulation run */
	*delete_point = current->next;
	if (current->value->array != NULL)
	  zfree((void*)current->value->array);
	zfree(current->value);
	zfree(current);
      }
      break;
    }
  }
}


STATIC_INLINE_DEVICE\
(void)
init_device_properties(device *me,
		       void *data)
{
  device_property_entry *property = me->properties;
  while (property != NULL) {
    /* now do the phandles */
    if (property->value->type == ihandle_property) {
      if (property->value->original != NULL) {
	const device_property *original = property->value->original;
	if (original->array == NULL) {
	  init_device_properties(original->owner, data);
	}
	ASSERT(original->array != NULL);
	device_set_property(me, property->value->name,
			    ihandle_property,
			    original->array, original->sizeof_array, NULL);
      }
      else {
	device_instance *instance =
	  device_create_instance(me, (char*)property->init_array);
	unsigned_cell ihandle = H2BE_cell(device_instance_to_external(instance));
	device_set_property(me, property->value->name,
			    ihandle_property,
			    &ihandle, sizeof(ihandle), NULL);
      }
    }
    property = property->next;
  }
}


INLINE_DEVICE\
(const device_property *)
device_next_property(const device_property *property)
{
  /* find the property in the list */
  device *owner = property->owner;
  device_property_entry *entry = owner->properties;
  while (entry != NULL && entry->value != property)
    entry = entry->next;
  /* now return the following property */
  ASSERT(entry != NULL); /* must be a member! */
  if (entry->next != NULL)
    return entry->next->value;
  else
    return NULL;
}

INLINE_DEVICE\
(const device_property *)
device_find_property(device *me,
		     const char *property)
{
  name_specifier spec;
  if (me == NULL) {
    return NULL;
  }
  else if (property == NULL || strcmp(property, "") == 0) {
    if (me->properties == NULL)
      return NULL;
    else
      return me->properties->value;
  }
  else if (split_property_specifier(property, &spec)) {
    me = split_find_device(me, &spec);
    if (spec.name == NULL) { /*got to root*/
      device_property_entry *entry = me->properties;
      while (entry != NULL) {
	if (strcmp(entry->value->name, spec.property) == 0)
	  return entry->value;
	entry = entry->next;
      }
    }
  }
  return NULL;
}

STATIC_INLINE_DEVICE\
(void)
device_add_array_property(device *me,
			  const char *property,
			  const void *array,
			  int sizeof_array)
{
  TRACE(trace_devices,
	("device_add_array_property(me=0x%lx, property=%s, ...)\n",
	 (long)me, property));
  device_add_property(me, property, array_property,
		      array, sizeof_array, array, sizeof_array,
		      NULL, permenant_object);
}

INLINE_DEVICE\
(void)
device_set_array_property(device *me,
			  const char *property,
			  const void *array,
			  int sizeof_array)
{
  TRACE(trace_devices,
	("device_set_array_property(me=0x%lx, property=%s, ...)\n",
	 (long)me, property));
  device_set_property(me, property, array_property, array, sizeof_array, NULL);
}

INLINE_DEVICE\
(const device_property *)
device_find_array_property(device *me,
			   const char *property)
{
  const device_property *node;
  TRACE(trace_devices,
	("device_find_integer(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != array_property)
    device_error(me, "property %s not found or of wrong type\n", property);
  return node;
}


STATIC_INLINE_DEVICE\
(void)
device_add_boolean_property(device *me,
			    const char *property,
			    int boolean)
{
  signed32 new_boolean = (boolean ? -1 : 0);
  TRACE(trace_devices,
	("device_add_boolean(me=0x%lx, property=%s, boolean=%d)\n",
	 (long)me, property, boolean));
  device_add_property(me, property, boolean_property,
		      &new_boolean, sizeof(new_boolean),
		      &new_boolean, sizeof(new_boolean),
		      NULL, permenant_object);
}

INLINE_DEVICE\
(int)
device_find_boolean_property(device *me,
			     const char *property)
{
  const device_property *node;
  unsigned_cell boolean;
  TRACE(trace_devices,
	("device_find_boolean(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != boolean_property)
    device_error(me, "property %s not found or of wrong type\n", property);
  ASSERT(sizeof(boolean) == node->sizeof_array);
  memcpy(&boolean, node->array, sizeof(boolean));
  return boolean;
}

STATIC_INLINE_DEVICE\
(void)
device_add_ihandle_property(device *me,
			    const char *property,
			    const char *path)
{
  TRACE(trace_devices,
	("device_add_ihandle_property(me=0x%lx, property=%s, path=%s)\n",
	 (long)me, property, path));
  device_add_property(me, property, ihandle_property,
		      path, strlen(path) + 1,
		      NULL, 0,
		      NULL, permenant_object);
}

INLINE_DEVICE\
(device_instance *)
device_find_ihandle_property(device *me,
			     const char *property)
{
  const device_property *node;
  unsigned_cell ihandle;
  device_instance *instance;
  TRACE(trace_devices,
	("device_find_ihandle_property(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == NULL || node->type != ihandle_property)
    device_error(me, "property %s not found or of wrong type\n", property);
  if (node->array == NULL)
    device_error(me, "property %s not yet initialized\n", property);
  ASSERT(sizeof(ihandle) == node->sizeof_array);
  memcpy(&ihandle, node->array, sizeof(ihandle));
  instance = external_to_device_instance(me, BE2H_cell(ihandle));
  ASSERT(instance != NULL);
  return instance;
}

STATIC_INLINE_DEVICE\
(void)
device_add_integer_property(device *me,
			    const char *property,
			    signed32 integer)
{
  TRACE(trace_devices,
	("device_add_integer_property(me=0x%lx, property=%s, integer=%ld)\n",
	 (long)me, property, (long)integer));
  H2BE(integer);
  device_add_property(me, property, integer_property,
		      &integer, sizeof(integer),
		      &integer, sizeof(integer),
		      NULL, permenant_object);
}

INLINE_DEVICE\
(signed_word)
device_find_integer_property(device *me,
			     const char *property)
{
  const device_property *node;
  signed32 integer;
  TRACE(trace_devices,
	("device_find_integer(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != integer_property)
    device_error(me, "property %s not found or of wrong type\n", property);
  ASSERT(sizeof(integer) == node->sizeof_array);
  memcpy(&integer, node->array, sizeof(integer));
  return BE2H_cell(integer);
}

STATIC_INLINE_DEVICE\
(void)
device_add_reg_property(device *me,
			const char *property,
			const void *reg,
			int sizeof_reg)
{
  TRACE(trace_devices,
	("device_add_reg_property(me=0x%lx, property=%s)\n",
	 (long)me, property));
  device_add_property(me, property, reg_property,
		      reg, sizeof_reg,
		      reg, sizeof_reg,
		      NULL, permenant_object);
}

INLINE_DEVICE\
(int)
device_find_reg_property(device *me,
			 const char *property,
			 int reg,
			 device_unit *address,
			 device_unit *size)
{
  const device_property *node;
  int i;
  unsigned nr_address_cells = (device_find_property(me, "../#address-cells") != NULL
			       ? device_find_integer_property(me, "../#address-cells")
			       : 1);
  unsigned sizeof_address_cells = nr_address_cells * sizeof(unsigned_cell);
  unsigned nr_size_cells = (device_find_property(me, "../#size-cells") != NULL
			    ? device_find_integer_property(me, "../#size-cells")
			    : 1);
  unsigned sizeof_size_cells = nr_size_cells * sizeof(unsigned_cell);
  unsigned sizeof_reg = (sizeof_address_cells + sizeof_size_cells);
  void *address_cells;
  void *size_cells;
  TRACE(trace_devices,
	("device_find_reg_property(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != reg_property)
    device_error(me, "property %s not found or of wrong type\n", property);
  if (node->sizeof_array % sizeof_reg != 0)
    device_error(me, "property %s contains incomplete phys-addr size pair\n", property);
  if (node->sizeof_array <= sizeof_reg * reg)
    return 0;

  /* copy the address out - converting as we go */
  address_cells = ((char*)node->array) + reg * sizeof_reg;
  memset(address, '\0', sizeof(*address));
  memcpy(address->cells, address_cells, sizeof_address_cells);
  address->nr_cells = nr_address_cells;
  for (i = 0; i < nr_address_cells; i++)
    BE2H(address->cells[i]);

  /* copy the address out - converting as we go */
  size_cells = ((char*)address_cells) + sizeof_address_cells;
  memset(size, '\0', sizeof(*size));
  memcpy(size->cells, size_cells, sizeof_size_cells);
  size->nr_cells = nr_size_cells;
  for (i = 0; i < nr_size_cells; i++)
    BE2H(size->cells[i]);

  return node->sizeof_array / sizeof_reg;
}

STATIC_INLINE_DEVICE\
(void)
device_add_string_property(device *me,
			   const char *property,
			   const char *string)
{
  TRACE(trace_devices,
	("device_add_property(me=0x%lx, property=%s, string=%s)\n",
	 (long)me, property, string));
  device_add_property(me, property, string_property,
		      string, strlen(string) + 1,
		      string, strlen(string) + 1,
		      NULL, permenant_object);
}

INLINE_DEVICE\
(const char *)
device_find_string_property(device *me,
			    const char *property)
{
  const device_property *node;
  const char *string;
  TRACE(trace_devices,
	("device_find_string(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != string_property)
    device_error(me, "property %s not found or of wrong type\n", property);
  string = node->array;
  ASSERT(strlen(string) + 1 == node->sizeof_array);
  return string;
}

STATIC_INLINE_DEVICE\
(void)
device_add_duplicate_property(device *me,
			      const char *property,
			      const device_property *original)
{
  TRACE(trace_devices,
	("device_add_duplicate_property(me=0x%lx, property=%s, ...)\n",
	 (long)me, property));
  if (original->disposition != permenant_object)
    device_error(me, "Can only duplicate permenant objects\n");
  device_add_property(me, property,
		      original->type,
		      original->array, original->sizeof_array,
		      original->array, original->sizeof_array,
		      original, permenant_object);
}



/* Device Hardware: */

INLINE_DEVICE\
(unsigned)
device_io_read_buffer(device *me,
		      void *dest,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      cpu *processor,
		      unsigned_word cia)
{
  if (me->callback->io.read_buffer == NULL)
    device_error(me, "no io_read_buffer method\n");
  return me->callback->io.read_buffer(me, dest, space,
				      addr, nr_bytes,
				      processor, cia);
}

INLINE_DEVICE\
(unsigned)
device_io_write_buffer(device *me,
		       const void *source,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  if (me->callback->io.write_buffer == NULL)
    device_error(me, "no io_write_buffer method\n");
  return me->callback->io.write_buffer(me, source, space,
				       addr, nr_bytes,
				       processor, cia);
}

INLINE_DEVICE\
(unsigned)
device_dma_read_buffer(device *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes)
{
  if (me->callback->dma.read_buffer == NULL)
    device_error(me, "no dma_read_buffer method\n");
  return me->callback->dma.read_buffer(me, dest, space,
				       addr, nr_bytes);
}

INLINE_DEVICE\
(unsigned)
device_dma_write_buffer(device *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			int violate_read_only_section)
{
  if (me->callback->dma.write_buffer == NULL)
    device_error(me, "no dma_write_buffer method\n");
  return me->callback->dma.write_buffer(me, source, space,
					addr, nr_bytes,
					violate_read_only_section);
}

INLINE_DEVICE\
(void)
device_attach_address(device *me,
		      const char *name,
		      attach_type attach,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *who) /*callback/default*/
{
  if (me->callback->address.attach == NULL)
    device_error(me, "no address_attach method\n");
  me->callback->address.attach(me, name, attach, space,
			       addr, nr_bytes, access, who);
}

INLINE_DEVICE\
(void)
device_detach_address(device *me,
		      const char *name,
		      attach_type attach,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *who) /*callback/default*/
{
  if (me->callback->address.detach == NULL)
    device_error(me, "no address_detach method\n");
  me->callback->address.detach(me, name, attach, space,
			       addr, nr_bytes, access, who);
}



/* Interrupts: */

INLINE_DEVICE(void)
device_interrupt_event(device *me,
		       int my_port,
		       int level,
		       cpu *processor,
		       unsigned_word cia)
{
  int found_an_edge = 0;
  device_interrupt_edge *edge;
  /* device's interrupt lines directly connected */
  for (edge = me->interrupt_destinations;
       edge != NULL;
       edge = edge->next) {
    if (edge->my_port == my_port) {
      if (edge->dest->callback->interrupt.event == NULL)
	device_error(me, "no interrupt method\n");
      edge->dest->callback->interrupt.event(edge->dest,
					    edge->dest_port,
					    me,
					    my_port,
					    level,
					    processor, cia);
      found_an_edge = 1;
    }
  }
  if (!found_an_edge) {
    device_error(me, "No interrupt edge for port %d\n", my_port);
  }
}

INLINE_DEVICE\
(void)
device_interrupt_attach(device *me,
			int my_port,
			device *dest,
			int dest_port,
			object_disposition disposition)
{
  attach_device_interrupt_edge(&me->interrupt_destinations,
			       my_port,
			       dest,
			       dest_port,
			       disposition);
}

INLINE_DEVICE\
(void)
device_interrupt_detach(device *me,
			int my_port,
			device *dest,
			int dest_port)
{
  detach_device_interrupt_edge(me,
			       &me->interrupt_destinations,
			       my_port,
			       dest,
			       dest_port);
}

INLINE_DEVICE\
(int)
device_interrupt_decode(device *me,
			const char *port_name)
{
  if (port_name == NULL || port_name[0] == '\0')
    return 0;
  if (isdigit(port_name[0])) {
    return strtoul(port_name, NULL, 0);
  }
  else {
    const device_interrupt_port_descriptor *ports =
      me->callback->interrupt.ports;
    if (ports != NULL) {
      while (ports->name != NULL) {
	if (ports->bound > ports->number) {
	  int len = strlen(ports->name);
	  if (strncmp(port_name, ports->name, len) == 0) {
	    if (port_name[len] == '\0')
	      return ports->number;
	    else if(isdigit(port_name[len])) {
	      int port = ports->number + strtoul(&port_name[len], NULL, 0);
	      if (port >= ports->bound)
		device_error(me, "Interrupt port %s out of range\n",
			     port_name);
	      return port;
	    }
	  }
	}
	else if (strcmp(port_name, ports->name) == 0)
	  return ports->number;
	ports++;
      }
    }
  }
  device_error(me, "Unreconized interrupt port %s\n", port_name);
  return 0;
}

INLINE_DEVICE\
(int)
device_interrupt_encode(device *me,
			int port_number,
			char *buf,
			int sizeof_buf)
{
  const device_interrupt_port_descriptor *ports = NULL;
  ports = me->callback->interrupt.ports;
  if (ports != NULL) {
    while (ports->name != NULL) {
      if (ports->bound > ports->number) {
	if (port_number >= ports->number
	    && port_number < ports->bound) {
	  strcpy(buf, ports->name);
	  sprintf(buf + strlen(buf), "%d", port_number - ports->number);
	  if (strlen(buf) >= sizeof_buf)
	    error("device_interrupt_encode:buffer overflow\n");
	  return strlen(buf);
	}
      }
      else {
	if (ports->number == port_number) {
	  if (strlen(ports->name) >= sizeof_buf)
	    error("device_interrupt_encode: buffer overflow\n");
	  strcpy(buf, ports->name);
	  return strlen(buf);
	}
      }
      ports++;
    }
  }
  sprintf(buf, "%d", port_number);
  if (strlen(buf) >= sizeof_buf)
    error("device_interrupt_encode: buffer overflow\n");
  return strlen(buf);
}



/* IOCTL: */

EXTERN_DEVICE\
(int)
device_ioctl(device *me,
	     cpu *processor,
	     unsigned_word cia,
	     ...)
{
  int status;
  va_list ap;
  va_start(ap, cia);
  if (me->callback->ioctl == NULL)
    device_error(me, "no ioctl method\n");
  status = me->callback->ioctl(me, processor, cia, ap);
  va_end(ap);
  return status;
}
      


/* I/O */

EXTERN_DEVICE\
(void volatile)
device_error(device *me,
	     const char *fmt,
	     ...)
{
  char message[1024];
  va_list ap;
  /* format the message */
  va_start(ap, fmt);
  vsprintf(message, fmt, ap);
  va_end(ap);
  /* sanity check */
  if (strlen(message) >= sizeof(message))
    error("device_error: buffer overflow\n");
  if (me == NULL)
    error("device: %s\n", message);
  else
    error("%s: %s\n", me->path, message);
  while(1);
}


/* Tree utilities: */

/* parse <non-white-space> */

STATIC_INLINE_DEVICE\
(const char *)
device_skip_token(const char *chp)
{
  while (!isspace(*chp) && *chp != '\0')
    chp++;
  while (isspace(*chp) && *chp != '\0')
    chp++;
  return chp;
}

/* parse <address> */

STATIC_INLINE_DEVICE\
(const char *)
device_parse_address(device *current,
		     device *bus,
		     const char *chp,
		     unsigned_cell *property,
		     int nr_address_cells)
{
  device_unit address;
  int i;
  if (device_decode_unit(bus, chp, &address) < 0)
    device_error(current, "invalid unit address in %s\n", chp);
  if (address.nr_cells != nr_address_cells)
    device_error(current, "decode_unit returned incorrect number of address cells\n");
  for (i = 0; i < address.nr_cells; i++) {
    *property++ = H2BE_cell(address.cells[i]);
  }
  return device_skip_token(chp);
}

/* parse <address> */

STATIC_INLINE_DEVICE\
(const char *)
device_parse_size(device *current,
		  device *bus,
		  const char *chp,
		  unsigned_cell *property,
		  int nr_size_cells)
{
  if (nr_size_cells > 0) {
    unsigned_cell size;
    property += nr_size_cells - 1;
    size = strtoul(chp, 0, 0);
    *property++ = H2BE_cell(size);
    chp = device_skip_token(chp);
  }
  return chp;
}

/* parse { <address> { ","? <size> } }* */

STATIC_INLINE_DEVICE\
(void)
device_tree_parse_reg_property(device *current,
			       const char *property_name,
			       const char *property_value)
{
  int nr_cells;
  unsigned_cell *property;
  const int nr_address_cells =
    (device_find_property(current, "../#address-cells") != NULL
     ? device_find_integer_property(current, "../#address-cells")
     : 1);
  const int nr_size_cells =
    (device_find_property(current, "../#size-cells") != NULL
     ? device_find_integer_property(current, "../#size-cells")
     : 1);
  /* determine the number of cells */
  {
    const char *chp = property_value;
    int nr_entries = 0;
    while (*chp != '\0') {
      nr_entries += 1;
      chp = device_skip_token(chp);
    }
    if (nr_entries % 2 != 0)
      device_error(current, "incorrect number of values for reg property %s\n",
		   property_value);
    nr_cells = (nr_address_cells + nr_size_cells) * (nr_entries / 2);
  }

  /* create a property of that size */
  property = zalloc(nr_cells * sizeof(*property));

  /* fill it in */
  {
    int cell = 0;
    const char *chp = property_value;
    while (cell < nr_cells) {
      chp = device_parse_address(current, current->parent,
				 chp, &property[cell], nr_address_cells);
      cell += nr_address_cells;
      chp = device_parse_size(current, current->parent,
			      chp, &property[cell], nr_size_cells);
      cell += nr_size_cells;
    }
  }

  /* create it */
  return device_add_reg_property(current, property_name,
				 property, sizeof(property[0]) * nr_cells);
}

/* Ensure that the first address found in the reg property matches
   anything that was specified as part of the devices name */

STATIC_INLINE_DEVICE\
(void)
device_tree_verify_reg_unit_address(device *current,
				    const char *property)
{
  device_unit address;
  device_unit dummy_size;
  device_find_reg_property(current, property, 0, &address, &dummy_size);
  if (memcmp(&current->unit_address, &address, sizeof(address)) != 0)
    device_error(current, "Unit address as specified by the \"%s\" property in conflict with the value previously specified in the devices path\n", property);
}


/* parse { <child-address> <parent-address> <child-size> }* */

STATIC_INLINE_DEVICE\
(void)
device_tree_parse_ranges_property(device *current,
				  const char *property_name,
				  const char *property_value)
{
  int nr_cells;
  unsigned_cell *property;
  const int nr_parent_address_cells =
    (device_find_property(current, "../#address-cells") != NULL
     ? device_find_integer_property(current, "../#address-cells")
     : 1);
  const int nr_child_address_cells =
    (device_find_property(current, "#address-cells") != NULL
     ? device_find_integer_property(current, "#address-cells")
     : 1);
  const int nr_child_size_cells =
    (device_find_property(current, "#size-cells") != NULL
     ? device_find_integer_property(current, "#size-cells")
     : 1);

  /* determine the number of cells */
  {
    const char *chp = property_value;
    int nr_entries = 0;
    while (*chp != '\0') {
      nr_entries += 1;
      chp = device_skip_token(chp);
    }
    if (nr_entries % 3 != 0)
      device_error(current, "incorrect number of values for reg property %s\n",
		   property_value);
    nr_cells = (nr_parent_address_cells
		+ nr_child_address_cells
		+ nr_child_size_cells) * (nr_entries / 3);
  }

  /* create a property of that size */
  property = zalloc(nr_cells * sizeof(*property));

  /* fill it in */
  {
    int cell = 0;
    const char *chp = property_value;
    while (cell < nr_cells) {
      chp = device_parse_address(current, current,
				 chp, &property[cell], nr_child_address_cells);
      cell += nr_child_address_cells;
      chp = device_parse_address(current, device_parent(current),
				 chp, &property[cell], nr_parent_address_cells);
      cell += nr_parent_address_cells;
      chp = device_parse_size(current, current->parent,
			      chp, &property[cell], nr_child_size_cells);
      cell += nr_child_size_cells;
    }
  }

  /* create it */
  return device_add_array_property(current, property_name,
				   property, sizeof(property[0]) * nr_cells);
}

EXTERN_DEVICE\
(device *)
device_tree_add_parsed(device *current,
		       const char *fmt,
		       ...)
{
  char device_specifier[1024];
  name_specifier spec;

  /* format the path */
  {
    va_list ap;
    va_start(ap, fmt);
    vsprintf(device_specifier, fmt, ap);
    va_end(ap);
    if (strlen(device_specifier) >= sizeof(device_specifier))
      error("device_tree_add_parsed: buffer overflow\n");
  }

  /* break it up */
  if (!split_device_specifier(device_specifier, &spec))
    device_error(current, "error parsing %s\n", device_specifier);

  /* fill our tree with its contents */
  current = split_find_device(current, &spec);

  /* add any additional devices as needed */
  if (spec.name != NULL) {
    do {
      current =
	device_template_create_device(current, spec.name, spec.unit, spec.args);
    } while (split_device_name(&spec));
  }

  /* is there an interrupt spec */
  if (spec.property == NULL
      && spec.value != NULL) {
    char *op = split_value(&spec);
    switch (op[0]) {
    case '>':
      {
	char *my_port_name = split_value(&spec);
	char *dest_port_name = split_value(&spec);
	char *dest_device_name = split_value(&spec);
	int my_port = device_interrupt_decode(current, my_port_name);
	device *dest = device_tree_find_device(current, dest_device_name);
	int dest_port;
	if (dest == NULL)
	  device_error(current, "Unknown interrupt destination device %s\n",
		       dest_device_name);
	dest_port = device_interrupt_decode(dest, dest_port_name);
	device_interrupt_attach(current,
				my_port,
				dest,
				dest_port,
				permenant_object);
      }
      break;
    default:
      device_error(current, "unreconised interrupt spec %s\n", spec.value);
      break;
    }
  }

  /* is there a property */
  if (spec.property != NULL) {
    if (strcmp(spec.value, "true") == 0)
      device_add_boolean_property(current, spec.property, 1);
    else if (strcmp(spec.value, "false") == 0)
      device_add_boolean_property(current, spec.property, 0);
    else {
      const device_property *property;
      switch (spec.value[0]) {
      case '*':
	{
	  spec.value++;
	  device_add_ihandle_property(current, spec.property, spec.value);
	}
	break;
      case '#':
	{
	  unsigned long ul;
	  spec.value++;
	  ul = strtoul(spec.value, &spec.value, 0);
	  device_add_integer_property(current, spec.property, ul);
	}
	break;
      case '[':
	{
	  unsigned8 words[1024];
	  char *curr = spec.value + 1;
	  int nr_words = 0;
	  while (1) {
	    char *next;
	    words[nr_words] = H2BE_1(strtoul(curr, &next, 0));
	    if (curr == next)
	      break;
	    curr = next;
	    nr_words += 1;
	  }
	  device_add_array_property(current, spec.property,
				    words, sizeof(words[0]) * nr_words);
	}
	break;
      case '{':
	{
	  unsigned_cell words[1024];
	  char *curr = spec.value + 1;
	  int nr_words = 0;
	  while (1) {
	    char *next;
	    words[nr_words] = H2BE_cell(strtoul(curr, &next, 0));
	    if (curr == next)
	      break;
	    curr = next;
	    nr_words += 1;
	  }
	  if (strcmp(spec.property, "reg") == 0
	      || strcmp(spec.property, "alternate-reg") == 0) {
	    device_add_reg_property(current, spec.property,
				    words, sizeof(words[0]) * nr_words);
	  }
	  else {
	    device_add_array_property(current, spec.property,
				      words, sizeof(words[0]) * nr_words);
	  }
	}
	break;
      case '"':
	spec.value++;
	device_add_string_property(current, spec.property, spec.value);
	break;
      case '!':
	spec.value++;
	property = device_find_property(current, spec.value);
	if (property == NULL)
	  device_error(current, "property %s not found\n", spec.value);
	device_add_duplicate_property(current,
				      spec.property,
				      property);
	break;
      default:
	if (strcmp(spec.property, "reg") == 0
	    || strcmp(spec.property, "alternate-reg") == 0){
	  device_tree_parse_reg_property(current, spec.property, spec.value);
	  if (strcmp(spec.property, "reg") == 0) {
	    device_tree_verify_reg_unit_address(current, spec.property);
	  }
	}
	else if (strcmp(spec.property, "ranges") == 0) {
	  device_tree_parse_ranges_property(current, spec.property, spec.value);
	}
	else if (isdigit(spec.value[0]) || spec.value[0] == '-' || spec.value[0] == '+') {
	  unsigned long ul = strtoul(spec.value, &spec.value, 0);
	  device_add_integer_property(current, spec.property, ul);
	}
	else
	  device_add_string_property(current, spec.property, spec.value);
	break;
      }
    }
  }
  return current;
}

INLINE_DEVICE\
(void)
device_tree_traverse(device *root,
		     device_tree_traverse_function *prefix,
		     device_tree_traverse_function *postfix,
		     void *data)
{
  device *child;
  if (prefix != NULL)
    prefix(root, data);
  for (child = root->children; child != NULL; child = child->sibling) {
    device_tree_traverse(child, prefix, postfix, data);
  }
  if (postfix != NULL)
    postfix(root, data);
}

STATIC_INLINE_DEVICE\
(void)
device_print_address(device *me,
		     device *bus,
		     const unsigned_cell *array,
		     int nr_address_cells)
{
  int i;
  device_unit address = { 0 };
  char buf[1024];
  for (i = 0; i < nr_address_cells; i++) {
    address.nr_cells = nr_address_cells;
    address.cells[i] = BE2H_cell(array[i]);
  }
  device_encode_unit(bus, &address, buf, sizeof(buf));
  printf_filtered(" %s", buf);
}

STATIC_INLINE_DEVICE\
(void)
device_print_size(device *me,
		  device *bus,
		  const unsigned_cell *array,
		  int nr_size_cells)
{
  if (nr_size_cells > 0) {
    unsigned_cell size = BE2H_cell(array[nr_size_cells-1]);
    printf_filtered(" %ld", (long)size);
  }
}

STATIC_INLINE_DEVICE\
(void)
device_print_reg_property(device *me,
			  const device_property *property)
{
  const int nr_address_cells =
    (device_find_property(me, "../#address-cells") != NULL
     ? device_find_integer_property(me, "../#address-cells")
     : 1);
  const int nr_size_cells =
    (device_find_property(me, "#size-cells") != NULL
     ? device_find_integer_property(me, "../#size-cells")
     : 1);
  const unsigned_cell *array = property->array;
  int cell;
  int nr_cells = (property->sizeof_array
		  / (nr_address_cells + nr_size_cells)
		  / sizeof(array[0]));
  if (property->sizeof_array
      % ((nr_address_cells + nr_size_cells) * sizeof(array[0])) != 0)
    device_error(me, "reg property size is not modula component size\n");
  cell = 0;
  while (cell < nr_cells) {
    device_print_address(me, me->parent, &array[cell], nr_address_cells);
    cell += nr_address_cells;
    device_print_size(me, me->parent, &array[cell], nr_size_cells);
    cell += nr_size_cells;
  }
}

STATIC_INLINE_DEVICE\
(void)
device_print_ranges_property(device *me,
			     const device_property *property)
{
  const int nr_parent_address_cells =
    (device_find_property(me, "../#address-cells") != NULL
     ? device_find_integer_property(me, "../#address-cells")
     : 1);
  const int nr_child_address_cells =
    (device_find_property(me, "#address-cells") != NULL
     ? device_find_integer_property(me, "#address-cells")
     : 1);
  const int nr_child_size_cells =
    (device_find_property(me, "#size-cells") != NULL
     ? device_find_integer_property(me, "#size-cells")
     : 1);
  const unsigned_cell *array = property->array;
  int cell;
  int nr_cells = (property->sizeof_array
		  / (nr_parent_address_cells
		     + nr_child_address_cells
		     + nr_child_size_cells)
		  / sizeof(array[0]));
  if (property->sizeof_array
      % ((nr_parent_address_cells + nr_child_address_cells + nr_child_size_cells) * sizeof(array[0])) != 0)
    device_error(me, "reg property size is not modula component size\n");
  cell = 0;
  while (cell < nr_cells) {
    device_print_address(me, me, &array[cell], nr_child_address_cells);
    cell += nr_child_address_cells;
    device_print_address(me, me->parent, &array[cell], nr_parent_address_cells);
    cell += nr_parent_address_cells;
    device_print_size(me, me->parent, &array[cell], nr_child_size_cells);
    cell += nr_child_size_cells;
  }
}

INLINE_DEVICE\
(void)
device_tree_print_device(device *me,
			 void *ignore_or_null)
{
  const device_property *property;
  device_interrupt_edge *interrupt_edge;
  /* output my name */
  printf_filtered("%s\n", me->path);
  /* properties */
  for (property = device_find_property(me, NULL);
       property != NULL;
       property = device_next_property(property)) {
    printf_filtered("%s/%s", me->path, property->name);
    if (property->original != NULL) {
      printf_filtered(" !");
      printf_filtered("%s/%s\n", property->original->owner->path,
		      property->original->name);
    }
    else {
      switch (property->type) {
      case array_property:
	{
	  if (strcmp(property->name, "reg") == 0
	      || strcmp(property->name, "alternate-reg") == 0) {
	    device_print_reg_property(me, property);
	  }
	  else if (strcmp(property->name, "ranges") == 0) {
	    device_print_ranges_property(me, property);
	  }
	  else if ((property->sizeof_array % sizeof(unsigned_cell)) == 0) {
	    unsigned_cell *w = (unsigned_cell*)property->array;
	    printf_filtered(" {");
	    while ((char*)w - (char*)property->array < property->sizeof_array) {
	      printf_filtered(" 0x%lx", BE2H_cell(*w));
	      w++;
	    }
	  }
	  else {
	    unsigned8 *w = (unsigned8*)property->array;
	    printf_filtered(" [");
	    while ((char*)w - (char*)property->array < property->sizeof_array) {
	      printf_filtered(" 0x%2x", BE2H_1(*w));
	      w++;
	    }
	  }
	  printf_filtered("\n");
	}
	break;
      case boolean_property:
	{
	  int b = device_find_boolean_property(me, property->name);
	  printf_filtered(" %s\n", b ? "true"  : "false");
	}
	break;
      case ihandle_property:
	{
	  if (property->array != NULL) {
	    device_instance *i = device_find_ihandle_property(me, property->name);
	    printf_filtered(" *%s\n", i->path);
	  }
	  else {
	    /* drats, the instance hasn't yet been created.  Grub
               around and find the path that will be used to create
               the ihandle */
	    device_property_entry *entry = me->properties;
	    while (entry->value != property) {
	      entry = entry->next;
	      ASSERT(entry != NULL);
	    }
	    ASSERT(entry->init_array != NULL);
	    printf_filtered(" *%s\n", (char*)entry->init_array);
	  }
	}
	break;
      case integer_property:
	{
	  unsigned_word w = device_find_integer_property(me, property->name);
	  printf_filtered(" 0x%lx\n", (unsigned long)w);
	}
	break;
      case reg_property:
	{
	  int reg;
	  device_unit phys;
	  device_unit size;
	  for (reg = 0;
	       device_find_reg_property(me, property->name, reg, &phys, &size);
	       reg++) {
	    char unit[32];
	    int i;
	    device_encode_unit(device_parent(me), &phys, unit, sizeof(unit));
	    printf_filtered(" %s", unit);
	    for (i = 0; i < size.nr_cells; i++)
	      printf_filtered("%s0x%lx\n", (i == 0 ? " " : ","),
			      (unsigned long)size.cells[i]);
	  }
	}
	break;
      case string_property:
	{
	  const char *s = device_find_string_property(me, property->name);
	  printf_filtered(" \"%s\n", s);
	}
	break;
      }
    }
  }
  /* interrupts */
  for (interrupt_edge = me->interrupt_destinations;
       interrupt_edge != NULL;
       interrupt_edge = interrupt_edge->next) {
    char src[32];
    char dst[32];
    device_interrupt_encode(me, interrupt_edge->my_port, src, sizeof(src));
    device_interrupt_encode(interrupt_edge->dest,
			    interrupt_edge->dest_port, dst, sizeof(dst));
    printf_filtered("%s > %s %s %s\n",
		    me->path,
		    src, dst,
		    interrupt_edge->dest->path);
  }
}

INLINE_DEVICE\
(device *)
device_tree_find_device(device *root,
			const char *path)
{
  device *node;
  name_specifier spec;
  TRACE(trace_device_tree,
	("device_tree_find_device_tree(root=0x%lx, path=%s)\n",
	 (long)root, path));
  /* parse the path */
  split_device_specifier(path, &spec);
  if (spec.value != NULL)
    return NULL; /* something wierd */

  /* now find it */
  node = split_find_device(root, &spec);
  if (spec.name != NULL)
    return NULL; /* not a leaf */

  return node;
}

INLINE_DEVICE\
(void)
device_usage(int verbose)
{
  if (verbose == 1) {
    const device_descriptor *const *table;
    int pos;
    printf_filtered("\n");
    printf_filtered("A device/property specifier has the form:\n");
    printf_filtered("\n");
    printf_filtered("  /path/to/a/device [ property-value ]\n");
    printf_filtered("\n");
    printf_filtered("and a possible device is\n");
    printf_filtered("\n");
    pos = 0;
    for (table = device_table; *table != NULL; table++) {
      const device_descriptor *descr;
      for (descr = *table; descr->name != NULL; descr++) {
	pos += strlen(descr->name) + 2;
	if (pos > 75) {
	  pos = strlen(descr->name) + 2;
	  printf_filtered("\n");
	}
	printf_filtered("  %s", descr->name);
      }
      printf_filtered("\n");
    }
  }
  if (verbose > 1) {
    const device_descriptor *const *table;
    printf_filtered("\n");
    printf_filtered("A device/property specifier (<spec>) has the format:\n");
    printf_filtered("\n");
    printf_filtered("  <spec> ::= <path> [ <value> ] ;\n");
    printf_filtered("  <path> ::= { <prefix> } { <node> \"/\" } <node> ;\n");
    printf_filtered("  <prefix> ::= ( | \"/\" | \"../\" | \"./\" ) ;\n");
    printf_filtered("  <node> ::= <name> [ \"@\" <unit> ] [ \":\" <args> ] ;\n");
    printf_filtered("  <unit> ::= <number> { \",\" <number> } ;\n");
    printf_filtered("\n");
    printf_filtered("Where:\n");
    printf_filtered("\n");
    printf_filtered("  <name>  is the name of a device (list below)\n");
    printf_filtered("  <unit>  is the unit-address relative to the parent bus\n");
    printf_filtered("  <args>  additional arguments used when creating the device\n");
    printf_filtered("  <value> ::= ( <number> # integer property\n");
    printf_filtered("              | \"[\" { <number> } # array property (byte)\n");
    printf_filtered("              | \"{\" { <number> } # array property (cell)\n");
    printf_filtered("              | [ \"true\" | \"false\" ] # boolean property\n");
    printf_filtered("              | \"*\" <path> # ihandle property\n");
    printf_filtered("              | \"!\" <path> # copy property\n");
    printf_filtered("              | \">\" [ <number> ] <path> # attach interrupt\n");
    printf_filtered("              | \"<\" <path> # attach child interrupt\n");
    printf_filtered("              | \"\\\"\" <text> # string property\n");
    printf_filtered("              | <text> # string property\n");
    printf_filtered("              ) ;\n");
    printf_filtered("\n");
    printf_filtered("And the following are valid device names:\n");
    printf_filtered("\n");
    for (table = device_table; *table != NULL; table++) {
      const device_descriptor *descr;
      for (descr = *table; descr->name != NULL; descr++) {
	printf_filtered("  %s:\n", descr->name);
	/* interrupt ports */
	if (descr->callbacks->interrupt.ports != NULL) {
	  const device_interrupt_port_descriptor *ports =
	    descr->callbacks->interrupt.ports;
	  printf_filtered("    interrupt ports:");
	  while (ports->name != NULL) {
	    printf_filtered(" %s", ports->name);
	    ports++;
	  }
	  printf_filtered("\n");
	}
	/* general info */
	if (descr->callbacks->usage != NULL)
	  descr->callbacks->usage(verbose);
      }
    }
  }
}



/* External representation */

INLINE_DEVICE\
(device *)
external_to_device(device *tree_member,
		   unsigned_cell phandle)
{
  device *root = device_tree_find_device(tree_member, "/");
  device *me = cap_internal(root->phandles, phandle);
  return me;
}

INLINE_DEVICE\
(unsigned_cell)
device_to_external(device *me)
{
  device *root = device_tree_find_device(me, "/");
  unsigned_cell phandle = cap_external(root->phandles, me);
  return phandle;
}

INLINE_DEVICE\
(device_instance *)
external_to_device_instance(device *tree_member,
			    unsigned_cell ihandle)
{
  device *root = device_tree_find_device(tree_member, "/");
  device_instance *instance = cap_internal(root->ihandles, ihandle);
  return instance;
}

INLINE_DEVICE\
(unsigned_cell)
device_instance_to_external(device_instance *instance)
{
  device *root = device_tree_find_device(instance->owner, "/");
  unsigned_cell ihandle = cap_external(root->ihandles, instance);
  return ihandle;
}

#endif /* _DEVICE_C_ */
