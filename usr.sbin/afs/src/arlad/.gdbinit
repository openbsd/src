#
# $Id: .gdbinit,v 1.1 2000/09/11 14:40:39 art Exp $
#


define listfcachenodes
set $foo = lrulist->head
while $foo != 0
print *(FCacheEntry *) $foo->data
set $foo = $foo->next
end
print 4711
end

document listfcachenodes
List all fcache nodes in the lru-list
end


define listfcachenodes_fid
set $foo = lrulist->head
while $foo != 0
print ((FCacheEntry *) $foo->data)->fid
set $foo = $foo->next
end
print 4711
end

document listfcachenodes_fid
List all fcache nodes's fids in the lru-list
end

define fcache_lru_num_nodes
set $bar = 0
set $foo = lrulist->head
while $foo != 0
set $bar = $bar + 1
set $foo = $foo->next
end
print $bar
end

document fcache_lru_num_nodes
Count number of nodes in the fcache lrulist
end

define fcache_lru_num_used_nodes
set $bar = 0
set $foo = lrulist->head
while $foo != 0
if ((FCacheEntry *) $foo->data)->flags.usedp != 0
set $bar = $bar + 1
end
set $foo = $foo->next
end
print $bar
end

document fcache_lru_num_used_nodes
Count number of USED nodes in the fcache lrulist
end

define lwp_ps_internal
set $lwp_ps_queue = $arg0
set $lwp_ps_counter = $lwp_ps_queue.count
set $lwp_ps_foo = $lwp_ps_queue->head
while $lwp_ps_counter != 0
  printf " name: %s", (char *) $lwp_ps_foo->name
  if $lwp_ps_foo == lwp_cpptr
    printf "                 RUNNING THREAD"
  end
  printf "\n"
  printf "  eventlist:"
  set $lwp_ps_envcnt = 0
  while $lwp_ps_envcnt < $lwp_ps_foo->eventcnt
    printf " %x",  $lwp_ps_foo->eventlist[$lwp_ps_envcnt]
    set $lwp_ps_envcnt = $lwp_ps_envcnt + 1
  end
  printf "\n"
  if $lwp_ps_foo == lwp_cpptr
    printf "  fp: 0x%x\n", $fp
    printf "  pc: 0x%x\n", $pc
    printf "  pointers on topstack added for completeness\n"
  end
  printf "  fp: 0x%x\n",  ((int *)$lwp_ps_foo->context->topstack)[2]
  printf "  pc: 0x%x\n",  ((int *)$lwp_ps_foo->context->topstack)[3]
  set $lwp_ps_foo = $lwp_ps_foo->next
  set $lwp_ps_counter = $lwp_ps_counter - 1
end
end

define lwp_ps
echo Runnable[0]\n
lwp_ps_internal runnable[0]
echo Runnable[1]\n
lwp_ps_internal runnable[1]
echo Runnable[2]\n
lwp_ps_internal runnable[2]
echo Runnable[3]\n
lwp_ps_internal runnable[3]
echo Runnable[4]\n
lwp_ps_internal runnable[4]
echo Blocked\n
lwp_ps_internal blocked
end

document lwp_ps
Print all processes, running or blocked
end
