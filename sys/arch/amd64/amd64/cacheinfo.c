#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/specialreg.h>

static char *print_cache_config(struct cpu_info *, int, char *, char *);
static char *print_tlb_config(struct cpu_info *, int, char *, char *);

static char *
print_cache_config(struct cpu_info *ci, int cache_tag, char *name, char *sep)
{
	struct x86_cache_info *cai = &ci->ci_cinfo[cache_tag];

	if (cai->cai_totalsize == 0)
		return sep;

	if (sep == NULL)
		printf("%s: ", ci->ci_dev->dv_xname);
	else
		printf("%s", sep);

	if (cai->cai_string != NULL)
		printf("%s ", cai->cai_string);
	else if (cai->cai_totalsize >= 1024*1024)
		printf("%dMB %db/line ", cai->cai_totalsize / 1024 / 1024,
		    cai->cai_linesize);
	else
		printf("%dKB %db/line ", cai->cai_totalsize / 1024,
		    cai->cai_linesize);

	switch (cai->cai_associativity) {
	case    0:
		printf("disabled");
		break;
	case    1:
		printf("direct-mapped");
		break;
	case 0xff:
		printf("fully associative");
		break;
	default:
		printf("%d-way", cai->cai_associativity);
		break;
	}

	if (name != NULL)
		printf(" %s", name);

	return ", ";
}

static char *
print_tlb_config(struct cpu_info *ci, int cache_tag, char *name, char *sep)
{
	struct x86_cache_info *cai = &ci->ci_cinfo[cache_tag];

	if (cai->cai_totalsize == 0)
		return sep;

	if (sep == NULL)
		printf("%s: ", ci->ci_dev->dv_xname);
	else
		printf("%s", sep);
	if (name != NULL)
		printf("%s ", name);

	if (cai->cai_string != NULL) {
		printf("%s", cai->cai_string);
	} else {
		if (cai->cai_linesize >= 1024*1024)
			printf("%d %dMB entries ", cai->cai_totalsize,
			    cai->cai_linesize / 1024 / 1024);
		else
			printf("%d %dKB entries ", cai->cai_totalsize,
			    cai->cai_linesize / 1024);
		switch (cai->cai_associativity) {
		case 0:
			printf("disabled");
			break;
		case 1:
			printf("direct-mapped");
			break;
		case 0xff:
			printf("fully associative");
			break;
		default:
			printf("%d-way", cai->cai_associativity);
			break;
		}
	}
	return ", ";
}

const struct x86_cache_info *
cache_info_lookup(const struct x86_cache_info *cai, u_int8_t desc)
{
	int i;

	for (i = 0; cai[i].cai_desc != 0; i++) {
		if (cai[i].cai_desc == desc)
			return (&cai[i]);
	}

	return (NULL);
}


static const struct x86_cache_info amd_cpuid_l2cache_assoc_info[] = {
	{ 0, 0x01,    1 },
	{ 0, 0x02,    2 },
	{ 0, 0x04,    4 },
	{ 0, 0x06,    8 },
	{ 0, 0x08,   16 },
	{ 0, 0x0f, 0xff },
	{ 0, 0x00,    0 },
};

void
amd_cpu_cacheinfo(struct cpu_info *ci)
{
	const struct x86_cache_info *cp;
	struct x86_cache_info *cai;
	int family, model;
	u_int descs[4];
	u_int lfunc;

	family = (ci->ci_signature >> 8) & 15;
	model = CPUID2MODEL(ci->ci_signature);

	/*
	 * K5 model 0 has none of this info.
	 */
	if (family == 5 && model == 0)
		return;

	/*
	 * Get extended values for K8 and up.
	 */
	if (family == 0xf) {
		family += (ci->ci_signature >> 20) & 0xff;
		model += (ci->ci_signature >> 16) & 0xf;
	}

	/*
	 * Determine the largest extended function value.
	 */
	CPUID(0x80000000, descs[0], descs[1], descs[2], descs[3]);
	lfunc = descs[0];

	/*
	 * Determine L1 cache/TLB info.
	 */
	if (lfunc < 0x80000005) {
		/* No L1 cache info available. */
		return;
	}

	CPUID(0x80000005, descs[0], descs[1], descs[2], descs[3]);

	/*
	 * K6-III and higher have large page TLBs.
	 */
	if ((family == 5 && model >= 9) || family >= 6) {
		cai = &ci->ci_cinfo[CAI_ITLB2];
		cai->cai_totalsize = AMD_L1_EAX_ITLB_ENTRIES(descs[0]);
		cai->cai_associativity = AMD_L1_EAX_ITLB_ASSOC(descs[0]);
		cai->cai_linesize = (4 * 1024 * 1024);

		cai = &ci->ci_cinfo[CAI_DTLB2];
		cai->cai_totalsize = AMD_L1_EAX_DTLB_ENTRIES(descs[0]);
		cai->cai_associativity = AMD_L1_EAX_DTLB_ASSOC(descs[0]);
		cai->cai_linesize = (4 * 1024 * 1024);
	}

	cai = &ci->ci_cinfo[CAI_ITLB];
	cai->cai_totalsize = AMD_L1_EBX_ITLB_ENTRIES(descs[1]);
	cai->cai_associativity = AMD_L1_EBX_ITLB_ASSOC(descs[1]);
	cai->cai_linesize = (4 * 1024);

	cai = &ci->ci_cinfo[CAI_DTLB];
	cai->cai_totalsize = AMD_L1_EBX_DTLB_ENTRIES(descs[1]);
	cai->cai_associativity = AMD_L1_EBX_DTLB_ASSOC(descs[1]);
	cai->cai_linesize = (4 * 1024);

	cai = &ci->ci_cinfo[CAI_DCACHE];
	cai->cai_totalsize = AMD_L1_ECX_DC_SIZE(descs[2]);
	cai->cai_associativity = AMD_L1_ECX_DC_ASSOC(descs[2]);
	cai->cai_linesize = AMD_L1_EDX_IC_LS(descs[2]);

	cai = &ci->ci_cinfo[CAI_ICACHE];
	cai->cai_totalsize = AMD_L1_EDX_IC_SIZE(descs[3]);
	cai->cai_associativity = AMD_L1_EDX_IC_ASSOC(descs[3]);
	cai->cai_linesize = AMD_L1_EDX_IC_LS(descs[3]);

	/*
	 * Determine L2 cache/TLB info.
	 */
	if (lfunc < 0x80000006) {
		/* No L2 cache info available. */
		return;
	}

	CPUID(0x80000006, descs[0], descs[1], descs[2], descs[3]);

	cai = &ci->ci_cinfo[CAI_L2CACHE];
	cai->cai_totalsize = AMD_L2_ECX_C_SIZE(descs[2]);
	cai->cai_associativity = AMD_L2_ECX_C_ASSOC(descs[2]);
	cai->cai_linesize = AMD_L2_ECX_C_LS(descs[2]);

	cp = cache_info_lookup(amd_cpuid_l2cache_assoc_info,
	    cai->cai_associativity);
	if (cp != NULL)
		cai->cai_associativity = cp->cai_associativity;
	else
		cai->cai_associativity = 0;	/* XXX Unknown/reserved */
}

void
x86_print_cacheinfo(struct cpu_info *ci)
{
	char *sep;

	sep = NULL;
	if (ci->ci_cinfo[CAI_ICACHE].cai_totalsize != 0 ||
	    ci->ci_cinfo[CAI_DCACHE].cai_totalsize != 0) {
		sep = print_cache_config(ci, CAI_ICACHE, "I-cache", NULL);
		sep = print_cache_config(ci, CAI_DCACHE, "D-cache", sep);
	}
	if (ci->ci_cinfo[CAI_L2CACHE].cai_totalsize != 0) {
		sep = print_cache_config(ci, CAI_L2CACHE, "L2 cache", sep);
		if (sep != NULL)
			printf("\n");
	}
	if (ci->ci_cinfo[CAI_ITLB].cai_totalsize != 0) {
		sep = print_tlb_config(ci, CAI_ITLB, "ITLB", NULL);
		sep = print_tlb_config(ci, CAI_ITLB2, NULL, sep);
		if (sep != NULL)
			printf("\n");
	}
	if (ci->ci_cinfo[CAI_DTLB].cai_totalsize != 0) {
		sep = print_tlb_config(ci, CAI_DTLB, "DTLB", NULL);
		sep = print_tlb_config(ci, CAI_DTLB2, NULL, sep);
		if (sep != NULL)
			printf("\n");
	}
}
