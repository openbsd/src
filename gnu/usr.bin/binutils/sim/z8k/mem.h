#define PAGE_SIZE (1<<PAGE_POWER)

int sim_read_byte PARAMS((sim_state_type *, sim_phys_addr_type));
unsigned int sim_read_short PARAMS((sim_state_type *, sim_phys_addr_type));
void sim_write_long PARAMS((sim_state_type *, sim_phys_addr_type,
			    int));
void sim_write_short PARAMS((sim_state_type *, sim_phys_addr_type, int));
void sim_write_byte PARAMS((sim_state_type *, sim_phys_addr_type, int));
