/*
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author: Monk.liu@amd.com
 */
#ifndef AMDGPU_VIRT_H
#define AMDGPU_VIRT_H

#define AMDGPU_SRIOV_CAPS_SRIOV_VBIOS  (1 << 0) /* vBIOS is sr-iov ready */
#define AMDGPU_SRIOV_CAPS_ENABLE_IOV   (1 << 1) /* sr-iov is enabled on this GPU */
#define AMDGPU_SRIOV_CAPS_IS_VF        (1 << 2) /* this GPU is a virtual function */
#define AMDGPU_PASSTHROUGH_MODE        (1 << 3) /* thw whole GPU is pass through for VM */
#define AMDGPU_SRIOV_CAPS_RUNTIME      (1 << 4) /* is out of full access mode */

struct amdgpu_mm_table {
	struct amdgpu_bo	*bo;
	uint32_t		*cpu_addr;
	uint64_t		gpu_addr;
};

#define AMDGPU_VF_ERROR_ENTRY_SIZE    16

/* struct error_entry - amdgpu VF error information. */
struct amdgpu_vf_error_buffer {
	struct rwlock lock;
	int read_count;
	int write_count;
	uint16_t code[AMDGPU_VF_ERROR_ENTRY_SIZE];
	uint16_t flags[AMDGPU_VF_ERROR_ENTRY_SIZE];
	uint64_t data[AMDGPU_VF_ERROR_ENTRY_SIZE];
};

/**
 * struct amdgpu_virt_ops - amdgpu device virt operations
 */
struct amdgpu_virt_ops {
	int (*req_full_gpu)(struct amdgpu_device *adev, bool init);
	int (*rel_full_gpu)(struct amdgpu_device *adev, bool init);
	int (*reset_gpu)(struct amdgpu_device *adev);
	int (*wait_reset)(struct amdgpu_device *adev);
	void (*trans_msg)(struct amdgpu_device *adev, u32 req, u32 data1, u32 data2, u32 data3);
};

/*
 * Firmware Reserve Frame buffer
 */
struct amdgpu_virt_fw_reserve {
	struct amdgim_pf2vf_info_header *p_pf2vf;
	struct amdgim_vf2pf_info_header *p_vf2pf;
	unsigned int checksum_key;
};
/*
 * Defination between PF and VF
 * Structures forcibly aligned to 4 to keep the same style as PF.
 */
#define AMDGIM_DATAEXCHANGE_OFFSET		(64 * 1024)

#define AMDGIM_GET_STRUCTURE_RESERVED_SIZE(total, u8, u16, u32, u64) \
		(total - (((u8)+3) / 4 + ((u16)+1) / 2 + (u32) + (u64)*2))

enum AMDGIM_FEATURE_FLAG {
	/* GIM supports feature of Error log collecting */
	AMDGIM_FEATURE_ERROR_LOG_COLLECT = 0x1,
	/* GIM supports feature of loading uCodes */
	AMDGIM_FEATURE_GIM_LOAD_UCODES   = 0x2,
	/* VRAM LOST by GIM */
	AMDGIM_FEATURE_GIM_FLR_VRAMLOST = 0x4,
};

struct amdgim_pf2vf_info_header {
	/* the total structure size in byte. */
	uint32_t size;
	/* version of this structure, written by the GIM */
	uint32_t version;
} __aligned(4);
struct  amdgim_pf2vf_info_v1 {
	/* header contains size and version */
	struct amdgim_pf2vf_info_header header;
	/* max_width * max_height */
	unsigned int uvd_enc_max_pixels_count;
	/* 16x16 pixels/sec, codec independent */
	unsigned int uvd_enc_max_bandwidth;
	/* max_width * max_height */
	unsigned int vce_enc_max_pixels_count;
	/* 16x16 pixels/sec, codec independent */
	unsigned int vce_enc_max_bandwidth;
	/* MEC FW position in kb from the start of visible frame buffer */
	unsigned int mecfw_kboffset;
	/* The features flags of the GIM driver supports. */
	unsigned int feature_flags;
	/* use private key from mailbox 2 to create chueksum */
	unsigned int checksum;
} __aligned(4);

struct  amdgim_pf2vf_info_v2 {
	/* header contains size and version */
	struct amdgim_pf2vf_info_header header;
	/* use private key from mailbox 2 to create chueksum */
	uint32_t checksum;
	/* The features flags of the GIM driver supports. */
	uint32_t feature_flags;
	/* max_width * max_height */
	uint32_t uvd_enc_max_pixels_count;
	/* 16x16 pixels/sec, codec independent */
	uint32_t uvd_enc_max_bandwidth;
	/* max_width * max_height */
	uint32_t vce_enc_max_pixels_count;
	/* 16x16 pixels/sec, codec independent */
	uint32_t vce_enc_max_bandwidth;
	/* MEC FW position in kb from the start of VF visible frame buffer */
	uint64_t mecfw_kboffset;
	/* MEC FW size in KB */
	uint32_t mecfw_ksize;
	/* UVD FW position in kb from the start of VF visible frame buffer */
	uint64_t uvdfw_kboffset;
	/* UVD FW size in KB */
	uint32_t uvdfw_ksize;
	/* VCE FW position in kb from the start of VF visible frame buffer */
	uint64_t vcefw_kboffset;
	/* VCE FW size in KB */
	uint32_t vcefw_ksize;
	uint32_t reserved[AMDGIM_GET_STRUCTURE_RESERVED_SIZE(256, 0, 0, (9 + sizeof(struct amdgim_pf2vf_info_header)/sizeof(uint32_t)), 3)];
} __aligned(4);


struct amdgim_vf2pf_info_header {
	/* the total structure size in byte. */
	uint32_t size;
	/*version of this structure, written by the guest */
	uint32_t version;
} __aligned(4);

struct amdgim_vf2pf_info_v1 {
	/* header contains size and version */
	struct amdgim_vf2pf_info_header header;
	/* driver version */
	char driver_version[64];
	/* driver certification, 1=WHQL, 0=None */
	unsigned int driver_cert;
	/* guest OS type and version: need a define */
	unsigned int os_info;
	/* in the unit of 1M */
	unsigned int fb_usage;
	/* guest gfx engine usage percentage */
	unsigned int gfx_usage;
	/* guest gfx engine health percentage */
	unsigned int gfx_health;
	/* guest compute engine usage percentage */
	unsigned int compute_usage;
	/* guest compute engine health percentage */
	unsigned int compute_health;
	/* guest vce engine usage percentage. 0xffff means N/A. */
	unsigned int vce_enc_usage;
	/* guest vce engine health percentage. 0xffff means N/A. */
	unsigned int vce_enc_health;
	/* guest uvd engine usage percentage. 0xffff means N/A. */
	unsigned int uvd_enc_usage;
	/* guest uvd engine usage percentage. 0xffff means N/A. */
	unsigned int uvd_enc_health;
	unsigned int checksum;
} __aligned(4);

struct amdgim_vf2pf_info_v2 {
	/* header contains size and version */
	struct amdgim_vf2pf_info_header header;
	uint32_t checksum;
	/* driver version */
	uint8_t driver_version[64];
	/* driver certification, 1=WHQL, 0=None */
	uint32_t driver_cert;
	/* guest OS type and version: need a define */
	uint32_t os_info;
	/* in the unit of 1M */
	uint32_t fb_usage;
	/* guest gfx engine usage percentage */
	uint32_t gfx_usage;
	/* guest gfx engine health percentage */
	uint32_t gfx_health;
	/* guest compute engine usage percentage */
	uint32_t compute_usage;
	/* guest compute engine health percentage */
	uint32_t compute_health;
	/* guest vce engine usage percentage. 0xffff means N/A. */
	uint32_t vce_enc_usage;
	/* guest vce engine health percentage. 0xffff means N/A. */
	uint32_t vce_enc_health;
	/* guest uvd engine usage percentage. 0xffff means N/A. */
	uint32_t uvd_enc_usage;
	/* guest uvd engine usage percentage. 0xffff means N/A. */
	uint32_t uvd_enc_health;
	uint32_t reserved[AMDGIM_GET_STRUCTURE_RESERVED_SIZE(256, 64, 0, (12 + sizeof(struct amdgim_vf2pf_info_header)/sizeof(uint32_t)), 0)];
} __aligned(4);

#define AMDGPU_FW_VRAM_VF2PF_VER 2
typedef struct amdgim_vf2pf_info_v2 amdgim_vf2pf_info ;

#define AMDGPU_FW_VRAM_VF2PF_WRITE(adev, field, val) \
	do { \
		((amdgim_vf2pf_info *)adev->virt.fw_reserve.p_vf2pf)->field = (val); \
	} while (0)

#define AMDGPU_FW_VRAM_VF2PF_READ(adev, field, val) \
	do { \
		(*val) = ((amdgim_vf2pf_info *)adev->virt.fw_reserve.p_vf2pf)->field; \
	} while (0)

#define AMDGPU_FW_VRAM_PF2VF_READ(adev, field, val) \
	do { \
		if (!adev->virt.fw_reserve.p_pf2vf) \
			*(val) = 0; \
		else { \
			if (adev->virt.fw_reserve.p_pf2vf->version == 1) \
				*(val) = ((struct amdgim_pf2vf_info_v1 *)adev->virt.fw_reserve.p_pf2vf)->field; \
			if (adev->virt.fw_reserve.p_pf2vf->version == 2) \
				*(val) = ((struct amdgim_pf2vf_info_v2 *)adev->virt.fw_reserve.p_pf2vf)->field; \
		} \
	} while (0)

/* GPU virtualization */
struct amdgpu_virt {
	uint32_t			caps;
	struct amdgpu_bo		*csa_obj;
	uint64_t			csa_vmid0_addr;
	bool chained_ib_support;
	uint32_t			reg_val_offs;
	struct amdgpu_irq_src		ack_irq;
	struct amdgpu_irq_src		rcv_irq;
	struct work_struct		flr_work;
	struct amdgpu_mm_table		mm_table;
	const struct amdgpu_virt_ops	*ops;
	struct amdgpu_vf_error_buffer   vf_errors;
	struct amdgpu_virt_fw_reserve	fw_reserve;
	uint32_t gim_feature;
};

#define AMDGPU_CSA_SIZE		(8 * 1024)

#define amdgpu_sriov_enabled(adev) \
((adev)->virt.caps & AMDGPU_SRIOV_CAPS_ENABLE_IOV)

#define amdgpu_sriov_vf(adev) \
((adev)->virt.caps & AMDGPU_SRIOV_CAPS_IS_VF)

#define amdgpu_sriov_bios(adev) \
((adev)->virt.caps & AMDGPU_SRIOV_CAPS_SRIOV_VBIOS)

#define amdgpu_sriov_runtime(adev) \
((adev)->virt.caps & AMDGPU_SRIOV_CAPS_RUNTIME)

#define amdgpu_passthrough(adev) \
((adev)->virt.caps & AMDGPU_PASSTHROUGH_MODE)

static inline bool is_virtual_machine(void)
{
#if defined(__amd64__) || defined(__i386__)
	return boot_cpu_has(X86_FEATURE_HYPERVISOR);
#else
	return false;
#endif
}

struct amdgpu_vm;

uint64_t amdgpu_csa_vaddr(struct amdgpu_device *adev);
bool amdgpu_virt_mmio_blocked(struct amdgpu_device *adev);
int amdgpu_allocate_static_csa(struct amdgpu_device *adev);
int amdgpu_map_static_csa(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			  struct amdgpu_bo_va **bo_va);
void amdgpu_free_static_csa(struct amdgpu_device *adev);
void amdgpu_virt_init_setting(struct amdgpu_device *adev);
uint32_t amdgpu_virt_kiq_rreg(struct amdgpu_device *adev, uint32_t reg);
void amdgpu_virt_kiq_wreg(struct amdgpu_device *adev, uint32_t reg, uint32_t v);
int amdgpu_virt_request_full_gpu(struct amdgpu_device *adev, bool init);
int amdgpu_virt_release_full_gpu(struct amdgpu_device *adev, bool init);
int amdgpu_virt_reset_gpu(struct amdgpu_device *adev);
int amdgpu_virt_wait_reset(struct amdgpu_device *adev);
int amdgpu_virt_alloc_mm_table(struct amdgpu_device *adev);
void amdgpu_virt_free_mm_table(struct amdgpu_device *adev);
int amdgpu_virt_fw_reserve_get_checksum(void *obj, unsigned long obj_size,
					unsigned int key,
					unsigned int chksum);
void amdgpu_virt_init_data_exchange(struct amdgpu_device *adev);

#endif
