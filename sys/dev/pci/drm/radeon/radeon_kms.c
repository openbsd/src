/*	$OpenBSD: radeon_kms.c,v 1.30 2014/10/25 14:20:09 kettenis Exp $	*/
/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
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
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm_fb_helper.h>
#include "radeon.h"
#include <dev/pci/drm/radeon_drm.h>
#include "radeon_asic.h"
#include <dev/pci/drm/drm_pciids.h>

#ifndef PCI_MEM_START
#define PCI_MEM_START	0
#endif

#ifndef PCI_MEM_END
#define PCI_MEM_END	0xffffffff
#endif

/* can't include radeon_drv.h due to duplicated defines in radeon_reg.h */

#include "vga.h"

#if NVGA > 0
extern int vga_console_attached;
#endif

#define DRIVER_NAME		"radeon"
#define DRIVER_DESC		"ATI Radeon"
#define DRIVER_DATE		"20080613"

#define KMS_DRIVER_MAJOR	2
#define KMS_DRIVER_MINOR	29
#define KMS_DRIVER_PATCHLEVEL	0

int	radeon_driver_irq_handler_kms(void *);
void	radeon_driver_irq_preinstall_kms(struct drm_device *);
int	radeon_driver_irq_postinstall_kms(struct drm_device *);
void	radeon_driver_irq_uninstall_kms(struct drm_device *d);

int	radeon_gem_object_init(struct drm_gem_object *);
void	radeon_gem_object_free(struct drm_gem_object *);
int	radeon_gem_object_open(struct drm_gem_object *, struct drm_file *);
void	radeon_gem_object_close(struct drm_gem_object *, struct drm_file *);

int	radeon_driver_unload_kms(struct drm_device *);
int	radeon_driver_load_kms(struct drm_device *, unsigned long);
int	radeon_info_ioctl(struct drm_device *, void *, struct drm_file *);
int	radeon_driver_firstopen_kms(struct drm_device *);
void	radeon_driver_lastclose_kms(struct drm_device *);
int	radeon_driver_open_kms(struct drm_device *, struct drm_file *);
void	radeon_driver_postclose_kms(struct drm_device *, struct drm_file *);
void	radeon_driver_preclose_kms(struct drm_device *, struct drm_file *);
u32	radeon_get_vblank_counter_kms(struct drm_device *, int);
int	radeon_enable_vblank_kms(struct drm_device *, int);
void	radeon_disable_vblank_kms(struct drm_device *, int);
int	radeon_get_vblank_timestamp_kms(struct drm_device *, int, int *,
	    struct timeval *, unsigned);
void	radeon_set_filp_rights(struct drm_device *, struct drm_file **,
	    struct drm_file *, uint32_t *);

int	radeondrm_ioctl_kms(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	radeon_ioctl_kms(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	radeon_dma_ioctl_kms(struct drm_device *, struct drm_dma *, struct drm_file *);

int	radeon_cp_init_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_start_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_stop_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_reset_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_idle_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_resume_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_engine_reset_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_fullscreen_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_swap_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_clear_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_vertex_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_indices_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_texture_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_stipple_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_indirect_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_vertex2_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_cmdbuf_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_getparam_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_flip_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_mem_alloc_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_mem_free_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_mem_init_heap_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_irq_emit_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_irq_wait_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_cp_setparam_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_surface_alloc_kms(struct drm_device *, void *, struct drm_file *);
int	radeon_surface_free_kms(struct drm_device *, void *, struct drm_file *);

int	radeondrm_probe(struct device *, void *, void *);
void	radeondrm_attach_kms(struct device *, struct device *, void *);
int	radeondrm_detach_kms(struct device *, int);
int	radeondrm_activate_kms(struct device *, int);
void	radeondrm_attachhook(void *);
int	radeondrm_forcedetach(struct radeon_device *);

struct cfattach radeondrm_ca = {
        sizeof (struct radeon_device), radeondrm_probe, radeondrm_attach_kms,
        radeondrm_detach_kms, radeondrm_activate_kms
};

struct cfdriver radeondrm_cd = { 
	NULL, "radeondrm", DV_DULL
};

/* Disable AGP writeback for scratch registers */
int radeon_no_wb;

/* Disable/Enable modesetting */
int radeon_modeset = 1;

/* Disable/Enable dynamic clocks */
int radeon_dynclks = -1;

/* Enable ATOMBIOS modesetting for R4xx */
int radeon_r4xx_atom = 0;

/* AGP Mode (-1 == PCI) */
int radeon_agpmode = 0;

/* Restrict VRAM for testing */
int radeon_vram_limit = 0;

/* Size of PCIE/IGP gart to setup in megabytes (32, 64, etc) */
int radeon_gart_size = 512; /* default gart size */

/* Run benchmark */
int radeon_benchmarking = 0;

/* Run tests */
int radeon_testing = 0;

/* Force connector table */
int radeon_connector_table = 0;

/* TV enable (0 = disable) */
int radeon_tv = 1;

/* Audio enable (1 = enable) */
int radeon_audio = 0;

/* Display Priority (0 = auto, 1 = normal, 2 = high) */
int radeon_disp_priority = 0;

/* hw i2c engine enable (0 = disable) */
int radeon_hw_i2c = 0;

/* PCIE Gen2 mode (-1 = auto, 0 = disable, 1 = enable) */
int radeon_pcie_gen2 = -1;

/* MSI support (1 = enable, 0 = disable, -1 = auto) */
int radeon_msi = -1;

/* GPU lockup timeout in ms (defaul 10000 = 10 seconds, 0 = disable) */
int radeon_lockup_timeout = 10000;

/*
 * set if the mountroot hook has a fatal error
 * such as not being able to find the firmware on newer cards
 */
int radeon_fatal_error = 0;

const struct drm_pcidev radeondrm_pciidlist[] = {
	radeon_PCI_IDS
};

static struct drm_driver_info kms_driver = {
	.flags =
	    DRIVER_AGP | DRIVER_MTRR | DRIVER_PCI_DMA | DRIVER_SG |
	    DRIVER_IRQ | DRIVER_DMA | DRIVER_GEM | DRIVER_MODESET,
	.buf_priv_size = 0,
	.ioctl = radeondrm_ioctl_kms,
	.firstopen = radeon_driver_firstopen_kms,
	.open = radeon_driver_open_kms,
	.mmap = radeon_mmap,
#ifdef notyet
	.preclose = radeon_driver_preclose_kms,
	.postclose = radeon_driver_postclose_kms,
#endif
	.lastclose = radeon_driver_lastclose_kms,
#ifdef notyet
	.suspend = radeon_suspend_kms,
	.resume = radeon_resume_kms,
#endif
	.get_vblank_counter = radeon_get_vblank_counter_kms,
	.enable_vblank = radeon_enable_vblank_kms,
	.disable_vblank = radeon_disable_vblank_kms,
	.get_vblank_timestamp = radeon_get_vblank_timestamp_kms,
	.get_scanout_position = radeon_get_crtc_scanoutpos,
#if defined(CONFIG_DEBUG_FS)
	.debugfs_init = radeon_debugfs_init,
	.debugfs_cleanup = radeon_debugfs_cleanup,
#endif
	.irq_preinstall = radeon_driver_irq_preinstall_kms,
	.irq_postinstall = radeon_driver_irq_postinstall_kms,
	.irq_uninstall = radeon_driver_irq_uninstall_kms,
	.irq_handler = radeon_driver_irq_handler_kms,
	.gem_init_object = radeon_gem_object_init,
	.gem_free_object = radeon_gem_object_free,
	.gem_open_object = radeon_gem_object_open,
	.gem_close_object = radeon_gem_object_close,
	.gem_size = sizeof(struct radeon_bo),
	.dma_ioctl = radeon_dma_ioctl_kms,
	.dumb_create = radeon_mode_dumb_create,
	.dumb_map_offset = radeon_mode_dumb_mmap,
	.dumb_destroy = radeon_mode_dumb_destroy,
#ifdef notyet
	.fops = &radeon_driver_kms_fops,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = radeon_gem_prime_export,
	.gem_prime_import = radeon_gem_prime_import,
#endif

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = KMS_DRIVER_MAJOR,
	.minor = KMS_DRIVER_MINOR,
	.patchlevel = KMS_DRIVER_PATCHLEVEL,
};

int
radeondrm_probe(struct device *parent, void *match, void *aux)
{
	if (radeon_fatal_error)
		return 0;
	if (drm_pciprobe(aux, radeondrm_pciidlist))
		return 20;
	return 0;
}

/**
 * radeon_driver_unload_kms - Main unload function for KMS.
 *
 * @dev: drm dev pointer
 *
 * This is the main unload function for KMS (all asics).
 * It calls radeon_modeset_fini() to tear down the
 * displays, and radeon_device_fini() to tear down
 * the rest of the device (CP, writeback, etc.).
 * Returns 0 on success.
 */
int
radeondrm_detach_kms(struct device *self, int flags)
{
	struct radeon_device *rdev = (struct radeon_device *)self;

	if (rdev == NULL)
		return 0;

	radeon_acpi_fini(rdev);
	radeon_modeset_fini(rdev);
	radeon_device_fini(rdev);

	if (rdev->ddev != NULL) {
		config_detach((struct device *)rdev->ddev, flags);
		rdev->ddev = NULL;
	}

	pci_intr_disestablish(rdev->pc, rdev->irqh);

	if (rdev->rmmio_size > 0)
		bus_space_unmap(rdev->memt, rdev->rmmio, rdev->rmmio_size);

	return 0;
}

int radeondrm_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t radeondrm_wsmmap(void *, off_t, int);
int radeondrm_alloc_screen(void *, const struct wsscreen_descr *,
    void **, int *, int *, long *);
void radeondrm_free_screen(void *, void *);
int radeondrm_show_screen(void *, void *, int,
    void (*)(void *, int, int), void *);
void radeondrm_doswitch(void *, void *);
#ifdef __sparc64__
void radeondrm_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
#endif

struct wsscreen_descr radeondrm_stdscreen = {
	"std",
	0, 0,
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *radeondrm_scrlist[] = {
	&radeondrm_stdscreen,
};

struct wsscreen_list radeondrm_screenlist = {
	nitems(radeondrm_scrlist), radeondrm_scrlist
};

struct wsdisplay_accessops radeondrm_accessops = {
	.ioctl = radeondrm_wsioctl,
	.mmap = radeondrm_wsmmap,
	.alloc_screen = radeondrm_alloc_screen,
	.free_screen = radeondrm_free_screen,
	.show_screen = radeondrm_show_screen,
	.getchar = rasops_getchar,
	.load_font = rasops_load_font,
	.list_font = rasops_list_font,
	.burn_screen = radeondrm_burner
};

int
radeondrm_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_RADEONDRM;
		return 0;
	default:
		return -1;
	}
}

paddr_t
radeondrm_wsmmap(void *v, off_t off, int prot)
{
	return (-1);
}

int
radeondrm_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	return rasops_alloc_screen(v, cookiep, curxp, curyp, attrp);
}

void
radeondrm_free_screen(void *v, void *cookie)
{
	return rasops_free_screen(v, cookie);
}

int
radeondrm_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct rasops_info *ri = v;
	struct radeon_device *rdev = ri->ri_hw;

	if (cookie == ri->ri_active)
		return (0);

	rdev->switchcb = cb;
	rdev->switchcbarg = cbarg;
	if (cb) {
		workq_queue_task(NULL, &rdev->switchwqt, 0,
		    radeondrm_doswitch, v, cookie);
		return (EAGAIN);
	}

	radeondrm_doswitch(v, cookie);

	return (0);
}

void
radeondrm_doswitch(void *v, void *cookie)
{
	struct rasops_info *ri = v;
	struct radeon_device *rdev = ri->ri_hw;
	struct radeon_crtc *radeon_crtc;
	int i, crtc;

	rasops_show_screen(ri, cookie, 0, NULL, NULL);
	for (crtc = 0; crtc < rdev->num_crtc; crtc++) {
		for (i = 0; i < 256; i++) {
			radeon_crtc = rdev->mode_info.crtcs[crtc];
			radeon_crtc->lut_r[i] = rasops_cmap[3 * i] << 2;
			radeon_crtc->lut_g[i] = rasops_cmap[(3 * i) + 1] << 2;
			radeon_crtc->lut_b[i] = rasops_cmap[(3 * i) + 2] << 2;
		}
	}
#ifdef __sparc64__
	fbwscons_setcolormap(&rdev->sf, radeondrm_setcolor);
#endif
	drm_fb_helper_restore();

	if (rdev->switchcb)
		(rdev->switchcb)(rdev->switchcbarg, 0, 0);
}

#ifdef __sparc64__
void
radeondrm_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct sunfb *sf = v;
	struct radeon_device *rdev = sf->sf_ro.ri_hw;
	struct drm_fb_helper *fb_helper = (void *)rdev->mode_info.rfbdev;
	u_int16_t red, green, blue;
	struct drm_crtc *crtc;
	int i;

	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;

		red = (r << 8) | r;
		green = (g << 8) | g;
		blue = (b << 8) | b;
		fb_helper->funcs->gamma_set(crtc, red, green, blue, index);
	}
}
#endif

/**
 * radeon_driver_load_kms - Main load function for KMS.
 *
 * @dev: drm dev pointer
 * @flags: device flags
 *
 * This is the main load function for KMS (all asics).
 * It calls radeon_device_init() to set up the non-display
 * parts of the chip (asic init, CP, writeback, etc.), and
 * radeon_modeset_init() to set up the display parts
 * (crtcs, encoders, hotplug detect, etc.).
 * Returns 0 on success, error on failure.
 */
void
radeondrm_attach_kms(struct device *parent, struct device *self, void *aux)
{
	struct radeon_device	*rdev = (struct radeon_device *)self;
	struct drm_device	*dev;
	struct pci_attach_args	*pa = aux;
	const struct drm_pcidev *id_entry;
	int			 is_agp;
	pcireg_t		 type;
	uint8_t			 iobar;
#if !defined(__sparc64__)
	pcireg_t		 addr, mask;
	int			 s;
#endif

#if defined(__sparc64__) || defined(__macppc__)
	extern int fbnode;
#endif

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), radeondrm_pciidlist);
	rdev->flags = id_entry->driver_data;
	rdev->pc = pa->pa_pc;
	rdev->pa_tag = pa->pa_tag;
	rdev->iot = pa->pa_iot;
	rdev->memt = pa->pa_memt;
	rdev->dmat = pa->pa_dmat;

#if defined(__sparc64__) || defined(__macppc__)
	if (fbnode == PCITAG_NODE(rdev->pa_tag))
		rdev->console = 1;
#else
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA &&
	    (pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    == (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE)) {
		rdev->console = 1;
#if NVGA > 0
		vga_console_attached = 1;
#endif
	}
#endif

#define RADEON_PCI_MEM		0x10
#define RADEON_PCI_IO		0x14
#define RADEON_PCI_MMIO		0x18
#define RADEON_PCI_IO2		0x20

	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, RADEON_PCI_MEM);
	if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
	    pci_mapreg_info(pa->pa_pc, pa->pa_tag, RADEON_PCI_MEM,
	    type, &rdev->fb_aper_offset, &rdev->fb_aper_size, NULL)) {
		printf(": can't get frambuffer info\n");
		return;
	}

	if (PCI_MAPREG_MEM_TYPE(type) != PCI_MAPREG_MEM_TYPE_64BIT)
		iobar = RADEON_PCI_IO;
	else
		iobar = RADEON_PCI_IO2;
	
	if (pci_mapreg_map(pa, iobar, PCI_MAPREG_TYPE_IO, 0,
	    NULL, &rdev->rio_mem, NULL, &rdev->rio_mem_size, 0)) {
		printf(": can't map IO space\n");
		return;
	}

	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, RADEON_PCI_MMIO);
	if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
	    pci_mapreg_map(pa, RADEON_PCI_MMIO, type, 0, NULL,
	    &rdev->rmmio, &rdev->rmmio_base, &rdev->rmmio_size, 0)) {
		printf(": can't map mmio space\n");
		return;
	}

#if !defined(__sparc64__)
	/*
	 * Make sure we have a base address for the ROM such that we
	 * can map it later.
	 */
	s = splhigh();
	addr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, ~PCI_ROM_ENABLE);
	mask = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, addr);
	splx(s);

	if (addr == 0 && PCI_ROM_SIZE(mask) != 0 && pa->pa_memex) {
		bus_size_t size, start, end;
		bus_addr_t base;

		size = PCI_ROM_SIZE(mask);
		start = max(PCI_MEM_START, pa->pa_memex->ex_start);
		end = min(PCI_MEM_END, pa->pa_memex->ex_end);
		if (extent_alloc_subregion(pa->pa_memex, start, end, size,
		    size, 0, 0, 0, &base) == 0)
			pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, base);
	}
#endif

#ifdef notyet
	mtx_init(&rdev->swi_lock, IPL_TTY);
#endif

	/* update BUS flag */
	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, NULL, NULL)) {
		rdev->flags |= RADEON_IS_AGP;
	} else if (pci_get_capability(pa->pa_pc, pa->pa_tag,
	    PCI_CAP_PCIEXPRESS, NULL, NULL)) {
		rdev->flags |= RADEON_IS_PCIE;
	} else {
		rdev->flags |= RADEON_IS_PCI;
	}

	DRM_DEBUG("%s card detected\n",
		 ((rdev->flags & RADEON_IS_AGP) ? "AGP" :
		 (((rdev->flags & RADEON_IS_PCIE) ? "PCIE" : "PCI"))));

	is_agp = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
	    NULL, NULL);

	printf("\n");

	dev = (struct drm_device *)drm_attach_pci(&kms_driver, pa, is_agp,
	    rdev->console, self);
	rdev->ddev = dev;
	rdev->pdev = dev->pdev;

	rdev->family = rdev->flags & RADEON_FAMILY_MASK;
	if (!radeon_msi_ok(rdev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	rdev->msi_enabled = 0;
	if (pci_intr_map_msi(pa, &rdev->intrh) == 0)
		rdev->msi_enabled = 1;
	else if (pci_intr_map(pa, &rdev->intrh) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}
	printf("%s: %s\n", rdev->dev.dv_xname,
	    pci_intr_string(pa->pa_pc, rdev->intrh));

	rdev->irqh = pci_intr_establish(pa->pa_pc, rdev->intrh, IPL_TTY,
	    radeon_driver_irq_handler_kms, rdev->ddev, rdev->dev.dv_xname);
	if (rdev->irqh == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    rdev->dev.dv_xname);
		return;
	}

#ifdef __sparc64__
{
	struct rasops_info *ri;
	int node, console;

	node = PCITAG_NODE(pa->pa_tag);
	console = (fbnode == node);

	fb_setsize(&rdev->sf, 8, 1152, 900, node, 0);

	/*
	 * The firmware sets up the framebuffer such that at starts at
	 * an offset from the start of video memory.
	 */
	rdev->fb_offset =
	    bus_space_read_4(rdev->memt, rdev->rmmio, RADEON_CRTC_OFFSET);
	if (bus_space_map(rdev->memt, rdev->fb_aper_offset + rdev->fb_offset,
	    rdev->sf.sf_fbsize, BUS_SPACE_MAP_LINEAR, &rdev->memh)) {
		printf("%s: can't map video memory\n", rdev->dev.dv_xname);
		return;
	}

	ri = &rdev->sf.sf_ro;
	ri->ri_bits = bus_space_vaddr(rdev->memt, rdev->memh);
	ri->ri_hw = rdev;
	ri->ri_updatecursor = NULL;

	fbwscons_init(&rdev->sf, RI_VCONS | RI_WRONLY | RI_BSWAP, console);
	if (console)
		fbwscons_console_init(&rdev->sf, -1);
}
#endif

	rdev->shutdown = true;
	if (rootvp == NULL)
		mountroothook_establish(radeondrm_attachhook, rdev);
	else
		radeondrm_attachhook(rdev);
}

int
radeondrm_forcedetach(struct radeon_device *rdev)
{
	struct pci_softc	*sc = (struct pci_softc *)rdev->dev.dv_parent;
	pcitag_t		 tag = rdev->pa_tag;

#if NVGA > 0
	if (rdev->console)
		vga_console_attached = 0;
#endif

	config_detach(&rdev->dev, 0);
	return pci_probe_device(sc, tag, NULL, NULL);
}

void
radeondrm_attachhook(void *xsc)
{
	struct radeon_device	*rdev = xsc;
	int			 r, acpi_status;

	/* radeon_device_init should report only fatal error
	 * like memory allocation failure or iomapping failure,
	 * or memory manager initialization failure, it must
	 * properly initialize the GPU MC controller and permit
	 * VRAM allocation
	 */
	r = radeon_device_init(rdev, rdev->ddev);
	if (r) {
		printf(": Fatal error during GPU init\n");
		radeon_fatal_error = 1;
		radeondrm_forcedetach(rdev);
		return;
	}

	/* Again modeset_init should fail only on fatal error
	 * otherwise it should provide enough functionalities
	 * for shadowfb to run
	 */
	r = radeon_modeset_init(rdev);
	if (r)
		printf("Fatal error during modeset init\n");

	/* Call ACPI methods: require modeset init
	 * but failure is not fatal
	 */
	if (!r) {
		acpi_status = radeon_acpi_init(rdev);
		if (acpi_status)
			DRM_DEBUG("Error during ACPI methods call\n");
	}

{
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &rdev->ro;

	if (ri->ri_bits == NULL)
		return;

#ifdef __sparc64__
	fbwscons_setcolormap(&rdev->sf, radeondrm_setcolor);
#endif
	drm_fb_helper_restore();

#ifndef __sparc64__
	ri->ri_flg = RI_CENTER | RI_VCONS | RI_WRONLY;
	rasops_init(ri, 160, 160);

	ri->ri_hw = rdev;
#else
	ri = &rdev->sf.sf_ro;
#endif

	radeondrm_stdscreen.capabilities = ri->ri_caps;
	radeondrm_stdscreen.nrows = ri->ri_rows;
	radeondrm_stdscreen.ncols = ri->ri_cols;
	radeondrm_stdscreen.textops = &ri->ri_ops;
	radeondrm_stdscreen.fontwidth = ri->ri_font->fontwidth;
	radeondrm_stdscreen.fontheight = ri->ri_font->fontheight;

	aa.console = rdev->console;
	aa.scrdata = &radeondrm_screenlist;
	aa.accessops = &radeondrm_accessops;
	aa.accesscookie = ri;
	aa.defaultscreens = 0;

	if (rdev->console) {
		long defattr;

		ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&radeondrm_stdscreen, ri->ri_active,
		    ri->ri_ccol, ri->ri_crow, defattr);
	}

	/*
	 * Now that we've taken over the console, disable decoding of
	 * VGA legacy addresses, and opt out of arbitration.
	 */
	radeon_vga_set_state(rdev, false);
	pci_disable_legacy_vga(&rdev->dev);

	printf("%s: %dx%d\n", rdev->dev.dv_xname, ri->ri_width, ri->ri_height);

	config_found_sm(&rdev->dev, &aa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);
}
}

int
radeondrm_activate_kms(struct device *self, int act)
{
	struct radeon_device *rdev = (struct radeon_device *)self;
	int rv = 0;

	if (rdev->ddev == NULL)
		return (0);

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		radeon_suspend_kms(rdev->ddev);
		break;
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		break;
	case DVACT_WAKEUP:
		radeon_resume_kms(rdev->ddev);
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

/**
 * radeon_set_filp_rights - Set filp right.
 *
 * @dev: drm dev pointer
 * @owner: drm file
 * @applier: drm file
 * @value: value
 *
 * Sets the filp rights for the device (all asics).
 */
void
radeon_set_filp_rights(struct drm_device *dev,
				   struct drm_file **owner,
				   struct drm_file *applier,
				   uint32_t *value)
{
	DRM_LOCK();
	if (*value == 1) {
		/* wants rights */
		if (!*owner)
			*owner = applier;
	} else if (*value == 0) {
		/* revokes rights */
		if (*owner == applier)
			*owner = NULL;
	}
	*value = *owner == applier ? 1 : 0;
	DRM_UNLOCK();
}

/*
 * Userspace get information ioctl
 */
/**
 * radeon_info_ioctl - answer a device specific request.
 *
 * @rdev: radeon device pointer
 * @data: request object
 * @filp: drm filp
 *
 * This function is used to pass device specific parameters to the userspace
 * drivers.  Examples include: pci device id, pipeline parms, tiling params,
 * etc. (all asics).
 * Returns 0 on success, -EINVAL on failure.
 */
int radeon_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct radeon_device *rdev = dev->dev_private;
	struct drm_radeon_info *info = data;
	struct radeon_mode_info *minfo = &rdev->mode_info;
	uint32_t value, *value_ptr;
	uint64_t value64, *value_ptr64;
	struct drm_crtc *crtc;
	int i, found;

	/* TIMESTAMP is a 64-bit value, needs special handling. */
	if (info->request == RADEON_INFO_TIMESTAMP) {
		if (rdev->family >= CHIP_R600) {
			value_ptr64 = (uint64_t*)((unsigned long)info->value);
			if (rdev->family >= CHIP_TAHITI) {
				value64 = si_get_gpu_clock(rdev);
			} else {
				value64 = r600_get_gpu_clock(rdev);
			}

			if (DRM_COPY_TO_USER(value_ptr64, &value64, sizeof(value64))) {
				DRM_ERROR("copy_to_user %s:%u\n", __func__, __LINE__);
				return -EFAULT;
			}
			return 0;
		} else {
			DRM_DEBUG_KMS("timestamp is r6xx+ only!\n");
			return -EINVAL;
		}
	}

	value_ptr = (uint32_t *)((unsigned long)info->value);
	if (DRM_COPY_FROM_USER(&value, value_ptr, sizeof(value))) {
		DRM_ERROR("copy_from_user %s:%u\n", __func__, __LINE__);
		return -EFAULT;
	}

	switch (info->request) {
	case RADEON_INFO_DEVICE_ID:
		value = dev->pci_device;
		break;
	case RADEON_INFO_NUM_GB_PIPES:
		value = rdev->num_gb_pipes;
		break;
	case RADEON_INFO_NUM_Z_PIPES:
		value = rdev->num_z_pipes;
		break;
	case RADEON_INFO_ACCEL_WORKING:
		/* xf86-video-ati 6.13.0 relies on this being false for evergreen */
		if ((rdev->family >= CHIP_CEDAR) && (rdev->family <= CHIP_HEMLOCK))
			value = false;
		else
			value = rdev->accel_working;
		break;
	case RADEON_INFO_CRTC_FROM_ID:
		for (i = 0, found = 0; i < rdev->num_crtc; i++) {
			crtc = (struct drm_crtc *)minfo->crtcs[i];
			if (crtc && crtc->base.id == value) {
				struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
				value = radeon_crtc->crtc_id;
				found = 1;
				break;
			}
		}
		if (!found) {
			DRM_DEBUG_KMS("unknown crtc id %d\n", value);
			return -EINVAL;
		}
		break;
	case RADEON_INFO_ACCEL_WORKING2:
		value = rdev->accel_working;
		break;
	case RADEON_INFO_TILING_CONFIG:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.tile_config;
		else if (rdev->family >= CHIP_CAYMAN)
			value = rdev->config.cayman.tile_config;
		else if (rdev->family >= CHIP_CEDAR)
			value = rdev->config.evergreen.tile_config;
		else if (rdev->family >= CHIP_RV770)
			value = rdev->config.rv770.tile_config;
		else if (rdev->family >= CHIP_R600)
			value = rdev->config.r600.tile_config;
		else {
			DRM_DEBUG_KMS("tiling config is r6xx+ only!\n");
			return -EINVAL;
		}
		break;
	case RADEON_INFO_WANT_HYPERZ:
		/* The "value" here is both an input and output parameter.
		 * If the input value is 1, filp requests hyper-z access.
		 * If the input value is 0, filp revokes its hyper-z access.
		 *
		 * When returning, the value is 1 if filp owns hyper-z access,
		 * 0 otherwise. */
		if (value >= 2) {
			DRM_DEBUG_KMS("WANT_HYPERZ: invalid value %d\n", value);
			return -EINVAL;
		}
		radeon_set_filp_rights(dev, &rdev->hyperz_filp, filp, &value);
		break;
	case RADEON_INFO_WANT_CMASK:
		/* The same logic as Hyper-Z. */
		if (value >= 2) {
			DRM_DEBUG_KMS("WANT_CMASK: invalid value %d\n", value);
			return -EINVAL;
		}
		radeon_set_filp_rights(dev, &rdev->cmask_filp, filp, &value);
		break;
	case RADEON_INFO_CLOCK_CRYSTAL_FREQ:
		/* return clock value in KHz */
		value = rdev->clock.spll.reference_freq * 10;
		break;
	case RADEON_INFO_NUM_BACKENDS:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.max_backends_per_se *
				rdev->config.si.max_shader_engines;
		else if (rdev->family >= CHIP_CAYMAN)
			value = rdev->config.cayman.max_backends_per_se *
				rdev->config.cayman.max_shader_engines;
		else if (rdev->family >= CHIP_CEDAR)
			value = rdev->config.evergreen.max_backends;
		else if (rdev->family >= CHIP_RV770)
			value = rdev->config.rv770.max_backends;
		else if (rdev->family >= CHIP_R600)
			value = rdev->config.r600.max_backends;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_NUM_TILE_PIPES:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.max_tile_pipes;
		else if (rdev->family >= CHIP_CAYMAN)
			value = rdev->config.cayman.max_tile_pipes;
		else if (rdev->family >= CHIP_CEDAR)
			value = rdev->config.evergreen.max_tile_pipes;
		else if (rdev->family >= CHIP_RV770)
			value = rdev->config.rv770.max_tile_pipes;
		else if (rdev->family >= CHIP_R600)
			value = rdev->config.r600.max_tile_pipes;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_FUSION_GART_WORKING:
		value = 1;
		break;
	case RADEON_INFO_BACKEND_MAP:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.backend_map;
		else if (rdev->family >= CHIP_CAYMAN)
			value = rdev->config.cayman.backend_map;
		else if (rdev->family >= CHIP_CEDAR)
			value = rdev->config.evergreen.backend_map;
		else if (rdev->family >= CHIP_RV770)
			value = rdev->config.rv770.backend_map;
		else if (rdev->family >= CHIP_R600)
			value = rdev->config.r600.backend_map;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_VA_START:
		/* this is where we report if vm is supported or not */
		if (rdev->family < CHIP_CAYMAN)
			return -EINVAL;
		value = RADEON_VA_RESERVED_SIZE;
		break;
	case RADEON_INFO_IB_VM_MAX_SIZE:
		/* this is where we report if vm is supported or not */
		if (rdev->family < CHIP_CAYMAN)
			return -EINVAL;
		value = RADEON_IB_VM_MAX_SIZE;
		break;
	case RADEON_INFO_MAX_PIPES:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.max_cu_per_sh;
		else if (rdev->family >= CHIP_CAYMAN)
			value = rdev->config.cayman.max_pipes_per_simd;
		else if (rdev->family >= CHIP_CEDAR)
			value = rdev->config.evergreen.max_pipes;
		else if (rdev->family >= CHIP_RV770)
			value = rdev->config.rv770.max_pipes;
		else if (rdev->family >= CHIP_R600)
			value = rdev->config.r600.max_pipes;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_MAX_SE:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.max_shader_engines;
		else if (rdev->family >= CHIP_CAYMAN)
			value = rdev->config.cayman.max_shader_engines;
		else if (rdev->family >= CHIP_CEDAR)
			value = rdev->config.evergreen.num_ses;
		else
			value = 1;
		break;
	case RADEON_INFO_MAX_SH_PER_SE:
		if (rdev->family >= CHIP_TAHITI)
			value = rdev->config.si.max_sh_per_se;
		else
			return -EINVAL;
		break;
	case RADEON_INFO_SI_CP_DMA_COMPUTE:
		value = 1;
		break;
	case RADEON_INFO_SI_BACKEND_ENABLED_MASK:
		if (rdev->family >= CHIP_TAHITI) {
			value = rdev->config.si.backend_enable_mask;
		} else {
			DRM_DEBUG_KMS("BACKEND_ENABLED_MASK is si+ only!\n");
		}
		break;
	default:
		DRM_DEBUG_KMS("Invalid request %d\n", info->request);
		return -EINVAL;
	}
	if (DRM_COPY_TO_USER(value_ptr, &value, sizeof(uint32_t))) {
		DRM_ERROR("copy_to_user %s:%u\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
}


/*
 * Outdated mess for old drm with Xorg being in charge (void function now).
 */
/**
 * radeon_driver_firstopen_kms - drm callback for first open
 *
 * @dev: drm dev pointer
 *
 * Nothing to be done for KMS (all asics).
 * Returns 0 on success.
 */
int radeon_driver_firstopen_kms(struct drm_device *dev)
{
	return 0;
}

/**
 * radeon_driver_firstopen_kms - drm callback for last close
 *
 * @dev: drm dev pointer
 *
 * Switch vga switcheroo state after last close (all asics).
 */
void radeon_driver_lastclose_kms(struct drm_device *dev)
{
#ifdef __sparc64__
	struct radeon_device *rdev = dev->dev_private;

	fbwscons_setcolormap(&rdev->sf, radeondrm_setcolor);
#endif
	drm_fb_helper_restore();
#ifdef notyet
	vga_switcheroo_process_delayed_switch();
#endif
}

/**
 * radeon_driver_open_kms - drm callback for open
 *
 * @dev: drm dev pointer
 * @file_priv: drm file
 *
 * On device open, init vm on cayman+ (all asics).
 * Returns 0 on success, error on failure.
 */
int radeon_driver_open_kms(struct drm_device *dev, struct drm_file *file_priv)
{
	struct radeon_device *rdev = dev->dev_private;

	file_priv->driver_priv = NULL;

	/* new gpu have virtual address space support */
	if (rdev->family >= CHIP_CAYMAN) {
		struct radeon_fpriv *fpriv;
		struct radeon_bo_va *bo_va;
		int r;

		fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
		if (unlikely(!fpriv)) {
			return -ENOMEM;
		}

		radeon_vm_init(rdev, &fpriv->vm);

		if (rdev->accel_working) {
			r = radeon_bo_reserve(rdev->ring_tmp_bo.bo, false);
			if (r) {
				radeon_vm_fini(rdev, &fpriv->vm);
				kfree(fpriv);
				return r;
			}

			/* map the ib pool buffer read only into
			 * virtual address space */
			bo_va = radeon_vm_bo_add(rdev, &fpriv->vm,
						 rdev->ring_tmp_bo.bo);
			r = radeon_vm_bo_set_addr(rdev, bo_va, RADEON_VA_IB_OFFSET,
						  RADEON_VM_PAGE_READABLE |
						  RADEON_VM_PAGE_SNOOPED);

			radeon_bo_unreserve(rdev->ring_tmp_bo.bo);
			if (r) {
				radeon_vm_fini(rdev, &fpriv->vm);
				kfree(fpriv);
				return r;
			}
		}
		file_priv->driver_priv = fpriv;
	}
	return 0;
}

/**
 * radeon_driver_postclose_kms - drm callback for post close
 *
 * @dev: drm dev pointer
 * @file_priv: drm file
 *
 * On device post close, tear down vm on cayman+ (all asics).
 */
void radeon_driver_postclose_kms(struct drm_device *dev,
				 struct drm_file *file_priv)
{
	struct radeon_device *rdev = dev->dev_private;

	/* new gpu have virtual address space support */
	if (rdev->family >= CHIP_CAYMAN && file_priv->driver_priv) {
		struct radeon_fpriv *fpriv = file_priv->driver_priv;
		struct radeon_bo_va *bo_va;
		int r;

		if (rdev->accel_working) {
			r = radeon_bo_reserve(rdev->ring_tmp_bo.bo, false);
			if (!r) {
				bo_va = radeon_vm_bo_find(&fpriv->vm,
							  rdev->ring_tmp_bo.bo);
				if (bo_va)
					radeon_vm_bo_rmv(rdev, bo_va);
				radeon_bo_unreserve(rdev->ring_tmp_bo.bo);
			}
		}

		radeon_vm_fini(rdev, &fpriv->vm);
		kfree(fpriv);
		file_priv->driver_priv = NULL;
	}
}

/**
 * radeon_driver_preclose_kms - drm callback for pre close
 *
 * @dev: drm dev pointer
 * @file_priv: drm file
 *
 * On device pre close, tear down hyperz and cmask filps on r1xx-r5xx
 * (all asics).
 */
void radeon_driver_preclose_kms(struct drm_device *dev,
				struct drm_file *file_priv)
{
	struct radeon_device *rdev = dev->dev_private;
	if (rdev->hyperz_filp == file_priv)
		rdev->hyperz_filp = NULL;
	if (rdev->cmask_filp == file_priv)
		rdev->cmask_filp = NULL;
}

/*
 * VBlank related functions.
 */
/**
 * radeon_get_vblank_counter_kms - get frame count
 *
 * @dev: drm dev pointer
 * @crtc: crtc to get the frame count from
 *
 * Gets the frame count on the requested crtc (all asics).
 * Returns frame count on success, -EINVAL on failure.
 */
u32 radeon_get_vblank_counter_kms(struct drm_device *dev, int crtc)
{
	struct radeon_device *rdev = dev->dev_private;

	if (crtc < 0 || crtc >= rdev->num_crtc) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	return radeon_get_vblank_counter(rdev, crtc);
}

/**
 * radeon_enable_vblank_kms - enable vblank interrupt
 *
 * @dev: drm dev pointer
 * @crtc: crtc to enable vblank interrupt for
 *
 * Enable the interrupt on the requested crtc (all asics).
 * Returns 0 on success, -EINVAL on failure.
 */
int radeon_enable_vblank_kms(struct drm_device *dev, int crtc)
{
	struct radeon_device *rdev = dev->dev_private;
	int r;

	if (crtc < 0 || crtc >= rdev->num_crtc) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	mtx_enter(&rdev->irq.lock);
	rdev->irq.crtc_vblank_int[crtc] = true;
	r = radeon_irq_set(rdev);
	mtx_leave(&rdev->irq.lock);
	return r;
}

/**
 * radeon_disable_vblank_kms - disable vblank interrupt
 *
 * @dev: drm dev pointer
 * @crtc: crtc to disable vblank interrupt for
 *
 * Disable the interrupt on the requested crtc (all asics).
 */
void radeon_disable_vblank_kms(struct drm_device *dev, int crtc)
{
	struct radeon_device *rdev = dev->dev_private;

	if (crtc < 0 || crtc >= rdev->num_crtc) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return;
	}

	mtx_enter(&rdev->irq.lock);
	rdev->irq.crtc_vblank_int[crtc] = false;
	radeon_irq_set(rdev);
	mtx_leave(&rdev->irq.lock);
}

/**
 * radeon_get_vblank_timestamp_kms - get vblank timestamp
 *
 * @dev: drm dev pointer
 * @crtc: crtc to get the timestamp for
 * @max_error: max error
 * @vblank_time: time value
 * @flags: flags passed to the driver
 *
 * Gets the timestamp on the requested crtc based on the
 * scanout position.  (all asics).
 * Returns postive status flags on success, negative error on failure.
 */
int radeon_get_vblank_timestamp_kms(struct drm_device *dev, int crtc,
				    int *max_error,
				    struct timeval *vblank_time,
				    unsigned flags)
{
	struct drm_crtc *drmcrtc;
	struct radeon_device *rdev = dev->dev_private;

	if (crtc < 0 || crtc >= dev->num_crtcs) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	/* Get associated drm_crtc: */
	drmcrtc = &rdev->mode_info.crtcs[crtc]->base;

	/* Helper routine in DRM core does all the work: */
	return drm_calc_vbltimestamp_from_scanoutpos(dev, crtc, max_error,
						     vblank_time, flags,
						     drmcrtc);
}

/*
 * IOCTL.
 */
int radeon_dma_ioctl_kms(struct drm_device *dev, struct drm_dma *d,
			 struct drm_file *file_priv)
{
	/* Not valid in KMS. */
	return -EINVAL;
}

#define KMS_INVALID_IOCTL(name)						\
int name(struct drm_device *dev, void *data, struct drm_file *file_priv)\
{									\
	DRM_ERROR("invalid ioctl with kms %s\n", __func__);		\
	return -EINVAL;							\
}

/*
 * All these ioctls are invalid in kms world.
 */
KMS_INVALID_IOCTL(radeon_cp_init_kms)
KMS_INVALID_IOCTL(radeon_cp_start_kms)
KMS_INVALID_IOCTL(radeon_cp_stop_kms)
KMS_INVALID_IOCTL(radeon_cp_reset_kms)
KMS_INVALID_IOCTL(radeon_cp_idle_kms)
KMS_INVALID_IOCTL(radeon_cp_resume_kms)
KMS_INVALID_IOCTL(radeon_engine_reset_kms)
KMS_INVALID_IOCTL(radeon_fullscreen_kms)
KMS_INVALID_IOCTL(radeon_cp_swap_kms)
KMS_INVALID_IOCTL(radeon_cp_clear_kms)
KMS_INVALID_IOCTL(radeon_cp_vertex_kms)
KMS_INVALID_IOCTL(radeon_cp_indices_kms)
KMS_INVALID_IOCTL(radeon_cp_texture_kms)
KMS_INVALID_IOCTL(radeon_cp_stipple_kms)
KMS_INVALID_IOCTL(radeon_cp_indirect_kms)
KMS_INVALID_IOCTL(radeon_cp_vertex2_kms)
KMS_INVALID_IOCTL(radeon_cp_cmdbuf_kms)
KMS_INVALID_IOCTL(radeon_cp_getparam_kms)
KMS_INVALID_IOCTL(radeon_cp_flip_kms)
KMS_INVALID_IOCTL(radeon_mem_alloc_kms)
KMS_INVALID_IOCTL(radeon_mem_free_kms)
KMS_INVALID_IOCTL(radeon_mem_init_heap_kms)
KMS_INVALID_IOCTL(radeon_irq_emit_kms)
KMS_INVALID_IOCTL(radeon_irq_wait_kms)
KMS_INVALID_IOCTL(radeon_cp_setparam_kms)
KMS_INVALID_IOCTL(radeon_surface_alloc_kms)
KMS_INVALID_IOCTL(radeon_surface_free_kms)

int
radeondrm_ioctl_kms(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	return -(radeon_ioctl_kms(dev, cmd, data, file_priv));
}

int
radeon_ioctl_kms(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_RADEON_CP_IDLE:
			return (radeon_cp_idle_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_RESUME:
			return (radeon_cp_resume_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_RESET:
			return (radeon_engine_reset_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_FULLSCREEN:
			return (radeon_fullscreen_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SWAP:
			return (radeon_cp_swap_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CLEAR:
			return (radeon_cp_clear_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_VERTEX:
			return (radeon_cp_vertex_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INDICES:
			return (radeon_cp_indices_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_TEXTURE:
			return (radeon_cp_texture_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_STIPPLE:
			return (radeon_cp_stipple_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_VERTEX2:
			return (radeon_cp_vertex2_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CMDBUF:
			return (radeon_cp_cmdbuf_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GETPARAM:
			return (radeon_cp_getparam_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_FLIP:
			return (radeon_cp_flip_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_ALLOC:
			return (radeon_mem_alloc_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_FREE:
			return (radeon_mem_free_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_IRQ_EMIT:
			return (radeon_irq_emit_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_IRQ_WAIT:
			return (radeon_irq_wait_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SETPARAM:
			return (radeon_cp_setparam_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SURF_ALLOC:
			return (radeon_surface_alloc_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SURF_FREE:
			return (radeon_surface_free_kms(dev, data, file_priv));
		/* KMS */
		case DRM_IOCTL_RADEON_GEM_INFO:
			return (radeon_gem_info_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_CREATE:
			return (radeon_gem_create_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_MMAP:
			return (radeon_gem_mmap_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_SET_DOMAIN:
			return (radeon_gem_set_domain_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_PREAD:
			return (radeon_gem_pread_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_PWRITE:
			return (radeon_gem_pwrite_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_WAIT_IDLE:
			return (radeon_gem_wait_idle_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CS:
			return (radeon_cs_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INFO:
			return (radeon_info_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_SET_TILING:
			return (radeon_gem_set_tiling_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_GET_TILING:
			return (radeon_gem_get_tiling_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_BUSY:
			return (radeon_gem_busy_ioctl(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GEM_VA:
			return (radeon_gem_va_ioctl(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_RADEON_CP_INIT:
			return (radeon_cp_init_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_START:
			return (radeon_cp_start_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_STOP:
			return (radeon_cp_stop_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_RESET:
			return (radeon_cp_reset_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INDIRECT:
			return (radeon_cp_indirect_kms(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INIT_HEAP:
			return (radeon_mem_init_heap_kms(dev, data, file_priv));
		}
	}
	return -EINVAL;
}
