/* ICU device: icu@<address>

   <address> : read - processor nr
   <address> : write - interrupt processor nr
   <address> + 4 : read - nr processors

   Single byte registers that control a simple ICU.

   Illustrates passing of events to parent device. Passing of
   interrupts to an interrupt destination. */


static unsigned
icu_io_read_buffer_callback(device *me,
			    void *dest,
			    int space,
			    unsigned_word addr,
			    unsigned nr_bytes,
			    cpu *processor,
			    unsigned_word cia)
{
  memset(dest, 0, nr_bytes);
  switch (addr & 4) {
  case 0:
    *(unsigned_1*)dest = cpu_nr(processor);
    break;
  case 4:
    *(unsigned_1*)dest =
      device_find_integer_property(me, "/openprom/options/smp");
    break;
  }
  return nr_bytes;
}


static unsigned
icu_io_write_buffer_callback(device *me,
			     const void *source,
			     int space,
			     unsigned_word addr,
			     unsigned nr_bytes,
			     cpu *processor,
			     unsigned_word cia)
{
  unsigned_1 val = H2T_1(*(unsigned_1*)source);
  /* tell the parent device that the interrupt lines have changed.
     For this fake ICU.  The interrupt lines just indicate the cpu to
     interrupt next */
  device_interrupt_event(me,
			 val, /*my_port*/
			 val, /*val*/
			 processor, cia);
  return nr_bytes;
}

static void
icu_do_interrupt(event_queue *queue,
		 void *data)
{
  cpu *target = (cpu*)data;
  /* try to interrupt the processor.  If the attempt fails, try again
     on the next tick */
  if (!external_interrupt(target))
    event_queue_schedule(queue, 1, icu_do_interrupt, target);
}


static void
icu_interrupt_event_callback(device *me,
			     int my_port,
			     device *source,
			     int source_port,
			     int level,
			     cpu *processor,
			     unsigned_word cia)
{
  /* the interrupt controller can't interrupt a cpu at any time.
     Rather it must synchronize with the system clock before
     performing an interrupt on the given processor */
  psim *system = cpu_system(processor);
  cpu *target = psim_cpu(system, my_port);
  if (target != NULL) {
    event_queue *events = cpu_event_queue(target);
    event_queue_schedule(events, 1, icu_do_interrupt, target);
  }
}

static device_callbacks const icu_callbacks = {
  { generic_device_init_address, },
  { NULL, }, /* address */
  { icu_io_read_buffer_callback,
    icu_io_write_buffer_callback, },
  { NULL, }, /* DMA */
  { icu_interrupt_event_callback, },
  { NULL, }, /* unit */
};


