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

#include "radeon_kfd.h"

#if defined(CONFIG_VGA_SWITCHEROO)
bool radeon_has_atpx(void);
#else
static inline bool radeon_has_atpx(void) { return false; }
#endif

#include "vga.h"

#if NVGA > 0
extern int vga_console_attached;
#endif

#ifdef __amd64__
#include "efifb.h"
#include <machine/biosvar.h>
#endif

#if NEFIFB > 0
#include <machine/efifbvar.h>
#endif

int	radeondrm_probe(struct device *, void *, void *);
void	radeondrm_attach_kms(struct device *, struct device *, void *);
int	radeondrm_detach_kms(struct device *, int);
int	radeondrm_activate_kms(struct device *, int);
void	radeondrm_attachhook(struct device *);
int	radeondrm_forcedetach(struct radeon_device *);

bool		radeon_msi_ok(struct radeon_device *);
irqreturn_t	radeon_driver_irq_handler_kms(void *);

extern const struct drm_pcidev radeondrm_pciidlist[];
extern struct drm_driver kms_driver;
const struct drm_ioctl_desc radeon_ioctls_kms[];
extern int radeon_max_kms_ioctl;

/*
 * set if the mountroot hook has a fatal error
 * such as not being able to find the firmware on newer cards
 */
int radeon_fatal_error;

struct cfattach radeondrm_ca = {
        sizeof (struct radeon_device), radeondrm_probe, radeondrm_attach_kms,
        radeondrm_detach_kms, radeondrm_activate_kms
};

struct cfdriver radeondrm_cd = { 
	NULL, "radeondrm", DV_DULL
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
#ifdef __linux__
int radeon_driver_unload_kms(struct drm_device *dev)
{
	struct radeon_device *rdev = dev->dev_private;

	if (rdev == NULL)
		return 0;

	if (rdev->rmmio == NULL)
		goto done_free;

	pm_runtime_get_sync(dev->dev);

	radeon_kfd_device_fini(rdev);

	radeon_acpi_fini(rdev);
	
	radeon_modeset_fini(rdev);
	radeon_device_fini(rdev);

done_free:
	kfree(rdev);
	dev->dev_private = NULL;
	return 0;
}
#else
int
radeondrm_detach_kms(struct device *self, int flags)
{
	struct radeon_device *rdev = (struct radeon_device *)self;

	if (rdev == NULL)
		return 0;

	pci_intr_disestablish(rdev->pc, rdev->irqh);

#ifdef notyet
	pm_runtime_get_sync(dev->dev);

	radeon_kfd_device_fini(rdev);
#endif

	radeon_acpi_fini(rdev);
	
	radeon_modeset_fini(rdev);
	radeon_device_fini(rdev);

	if (rdev->ddev != NULL) {
		config_detach((struct device *)rdev->ddev, flags);
		rdev->ddev = NULL;
	}

	return 0;
}
#endif

void radeondrm_burner(void *, u_int, u_int);
int radeondrm_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t radeondrm_wsmmap(void *, off_t, int);
int radeondrm_alloc_screen(void *, const struct wsscreen_descr *,
    void **, int *, int *, long *);
void radeondrm_free_screen(void *, void *);
int radeondrm_show_screen(void *, void *, int,
    void (*)(void *, int, int), void *);
void radeondrm_doswitch(void *);
void radeondrm_enter_ddb(void *, void *);
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
	.enter_ddb = radeondrm_enter_ddb,
	.getchar = rasops_getchar,
	.load_font = rasops_load_font,
	.list_font = rasops_list_font,
	.scrollback = rasops_scrollback,
	.burn_screen = radeondrm_burner
};

int
radeondrm_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct rasops_info *ri = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_RADEONDRM;
		return 0;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;
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
	rdev->switchcookie = cookie;
	if (cb) {
		task_add(systq, &rdev->switchtask);
		return (EAGAIN);
	}

	radeondrm_doswitch(v);

	return (0);
}

void
radeondrm_doswitch(void *v)
{
	struct rasops_info *ri = v;
	struct radeon_device *rdev = ri->ri_hw;
	struct radeon_crtc *radeon_crtc;
	int i, crtc;

	rasops_show_screen(ri, rdev->switchcookie, 0, NULL, NULL);
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
	drm_fb_helper_restore_fbdev_mode_unlocked((void *)rdev->mode_info.rfbdev);

	if (rdev->switchcb)
		(rdev->switchcb)(rdev->switchcbarg, 0, 0);
}

void
radeondrm_enter_ddb(void *v, void *cookie)
{
	struct rasops_info *ri = v;
	struct radeon_device *rdev = ri->ri_hw;
	struct drm_fb_helper *fb_helper = (void *)rdev->mode_info.rfbdev;

	if (cookie == ri->ri_active)
		return;

	rasops_show_screen(ri, cookie, 0, NULL, NULL);
	drm_fb_helper_debug_enter(fb_helper->fbdev);
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

#ifdef __linux__
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
int radeon_driver_load_kms(struct drm_device *dev, unsigned long flags)
{
	struct radeon_device *rdev;
	int r, acpi_status;

	rdev = kzalloc(sizeof(struct radeon_device), GFP_KERNEL);
	if (rdev == NULL) {
		return -ENOMEM;
	}
	dev->dev_private = (void *)rdev;

	/* update BUS flag */
	if (drm_pci_device_is_agp(dev)) {
		flags |= RADEON_IS_AGP;
	} else if (pci_is_pcie(dev->pdev)) {
		flags |= RADEON_IS_PCIE;
	} else {
		flags |= RADEON_IS_PCI;
	}

	if ((radeon_runtime_pm != 0) &&
	    radeon_has_atpx() &&
	    ((flags & RADEON_IS_IGP) == 0))
		flags |= RADEON_IS_PX;

	/* radeon_device_init should report only fatal error
	 * like memory allocation failure or iomapping failure,
	 * or memory manager initialization failure, it must
	 * properly initialize the GPU MC controller and permit
	 * VRAM allocation
	 */
	r = radeon_device_init(rdev, dev, dev->pdev, flags);
	if (r) {
		dev_err(&dev->pdev->dev, "Fatal error during GPU init\n");
		goto out;
	}

	/* Again modeset_init should fail only on fatal error
	 * otherwise it should provide enough functionalities
	 * for shadowfb to run
	 */
	r = radeon_modeset_init(rdev);
	if (r)
		dev_err(&dev->pdev->dev, "Fatal error during modeset init\n");

	/* Call ACPI methods: require modeset init
	 * but failure is not fatal
	 */
	if (!r) {
		acpi_status = radeon_acpi_init(rdev);
		if (acpi_status)
		dev_dbg(&dev->pdev->dev,
				"Error during ACPI methods call\n");
	}

#ifdef notyet
	radeon_kfd_device_probe(rdev);
	radeon_kfd_device_init(rdev);
#endif

	if (radeon_is_px(dev)) {
		pm_runtime_use_autosuspend(dev->dev);
		pm_runtime_set_autosuspend_delay(dev->dev, 5000);
		pm_runtime_set_active(dev->dev);
		pm_runtime_allow(dev->dev);
		pm_runtime_mark_last_busy(dev->dev);
		pm_runtime_put_autosuspend(dev->dev);
	}

out:
	if (r)
		radeon_driver_unload_kms(dev);


	return r;
}
#endif

void
radeondrm_attach_kms(struct device *parent, struct device *self, void *aux)
{
	struct radeon_device	*rdev = (struct radeon_device *)self;
	struct drm_device	*dev;
	struct pci_attach_args	*pa = aux;
	const struct drm_pcidev *id_entry;
	int			 is_agp;
	pcireg_t		 type;
	int			 i;
	uint8_t			 rmmio_bar;
	paddr_t			 fb_aper;
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
	rdev->family = rdev->flags & RADEON_FAMILY_MASK;
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
#if NEFIFB > 0
	if (efifb_is_console(pa)) {
		rdev->console = 1;
		efifb_cndetach();
	}
#endif
#endif

#define RADEON_PCI_MEM		0x10

	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, RADEON_PCI_MEM);
	if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
	    pci_mapreg_info(pa->pa_pc, pa->pa_tag, RADEON_PCI_MEM,
	    type, &rdev->fb_aper_offset, &rdev->fb_aper_size, NULL)) {
		printf(": can't get frambuffer info\n");
		return;
	}
#if !defined(__sparc64__)
	if (rdev->fb_aper_offset == 0) {
		bus_size_t start, end;
		bus_addr_t base;

		start = max(PCI_MEM_START, pa->pa_memex->ex_start);
		end = min(PCI_MEM_END, pa->pa_memex->ex_end);
		if (pa->pa_memex == NULL ||
		    extent_alloc_subregion(pa->pa_memex, start, end,
		    rdev->fb_aper_size, rdev->fb_aper_size, 0, 0, 0, &base)) {
			printf(": can't reserve framebuffer space\n");
			return;
		}
		pci_conf_write(pa->pa_pc, pa->pa_tag, RADEON_PCI_MEM, base);
		if (PCI_MAPREG_MEM_TYPE(type) == PCI_MAPREG_MEM_TYPE_64BIT)
			pci_conf_write(pa->pa_pc, pa->pa_tag,
			    RADEON_PCI_MEM + 4, (uint64_t)base >> 32);
		rdev->fb_aper_offset = base;
	}
#endif

	for (i = PCI_MAPREG_START; i < PCI_MAPREG_END ; i+= 4) {
		type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, i);
		if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_IO)
			continue;
		if (pci_mapreg_map(pa, i, type, 0, NULL,
		    &rdev->rio_mem, NULL, &rdev->rio_mem_size, 0)) {
			printf(": can't map rio space\n");
			return;
		}

		if (type & PCI_MAPREG_MEM_TYPE_64BIT)
			i += 4;
	}

	if (rdev->family >= CHIP_BONAIRE) {
		type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, 0x18);
		if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
		    pci_mapreg_map(pa, 0x18, type, 0, NULL,
		    &rdev->doorbell.bsh, &rdev->doorbell.base,
		    &rdev->doorbell.size, 0)) {
			printf(": can't map doorbell space\n");
			return;
		}
	}

	if (rdev->family >= CHIP_BONAIRE)
		rmmio_bar = 0x24;
	else
		rmmio_bar = 0x18;

	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, rmmio_bar);
	if (PCI_MAPREG_TYPE(type) != PCI_MAPREG_TYPE_MEM ||
	    pci_mapreg_map(pa, rmmio_bar, type, 0, NULL,
	    &rdev->rmmio_bsh, &rdev->rmmio_base, &rdev->rmmio_size, 0)) {
		printf(": can't map rmmio space\n");
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

	if ((radeon_runtime_pm != 0) &&
	    radeon_has_atpx() &&
	    ((rdev->flags & RADEON_IS_IGP) == 0))
		rdev->flags |= RADEON_IS_PX;

	DRM_DEBUG("%s card detected\n",
		 ((rdev->flags & RADEON_IS_AGP) ? "AGP" :
		 (((rdev->flags & RADEON_IS_PCIE) ? "PCIE" : "PCI"))));

	is_agp = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
	    NULL, NULL);

	printf("\n");

	kms_driver.num_ioctls = radeon_max_kms_ioctl;
	kms_driver.driver_features |= DRIVER_MODESET;

	dev = (struct drm_device *)drm_attach_pci(&kms_driver, pa, is_agp,
	    rdev->console, self);
	rdev->ddev = dev;
	rdev->pdev = dev->pdev;

	if (!radeon_msi_ok(rdev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	rdev->msi_enabled = 0;
	if (pci_intr_map_msi(pa, &rdev->intrh) == 0)
		rdev->msi_enabled = 1;
	else if (pci_intr_map(pa, &rdev->intrh) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}
	printf("%s: %s\n", rdev->self.dv_xname,
	    pci_intr_string(pa->pa_pc, rdev->intrh));

	rdev->irqh = pci_intr_establish(pa->pa_pc, rdev->intrh, IPL_TTY,
	    radeon_driver_irq_handler_kms, rdev->ddev, rdev->self.dv_xname);
	if (rdev->irqh == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    rdev->self.dv_xname);
		return;
	}
	rdev->pdev->irq = -1;

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
	    bus_space_read_4(rdev->memt, rdev->rmmio_bsh, RADEON_CRTC_OFFSET);
	if (bus_space_map(rdev->memt, rdev->fb_aper_offset + rdev->fb_offset,
	    rdev->sf.sf_fbsize, BUS_SPACE_MAP_LINEAR, &rdev->memh)) {
		printf("%s: can't map video memory\n", rdev->self.dv_xname);
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

	fb_aper = bus_space_mmap(rdev->memt, rdev->fb_aper_offset, 0, 0, 0);
	if (fb_aper != -1)
		rasops_claim_framebuffer(fb_aper, rdev->fb_aper_size, self);

	rdev->shutdown = true;
	config_mountroot(self, radeondrm_attachhook);
}

extern void mainbus_efifb_reattach(void);

int
radeondrm_forcedetach(struct radeon_device *rdev)
{
	struct pci_softc	*sc = (struct pci_softc *)rdev->self.dv_parent;
	pcitag_t		 tag = rdev->pa_tag;

#if NVGA > 0
	if (rdev->console)
		vga_console_attached = 0;
#endif

	/* reprobe pci device for non efi systems */
#if NEFIFB > 0
	if (bios_efiinfo == NULL && !efifb_cb_found()) {
#endif
		config_detach(&rdev->self, 0);
		return pci_probe_device(sc, tag, NULL, NULL);
#if NEFIFB > 0
	} else if (rdev->console) {
		mainbus_efifb_reattach();
	}
#endif

	return 0;
}

void
radeondrm_attachhook(struct device *self)
{
	struct radeon_device	*rdev = (struct radeon_device *)self;
	int			 r, acpi_status;

	/* radeon_device_init should report only fatal error
	 * like memory allocation failure or iomapping failure,
	 * or memory manager initialization failure, it must
	 * properly initialize the GPU MC controller and permit
	 * VRAM allocation
	 */
	r = radeon_device_init(rdev, rdev->ddev, rdev->ddev->pdev, rdev->flags);
	if (r) {
		dev_err(&dev->pdev->dev, "Fatal error during GPU init\n");
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
		dev_err(&dev->pdev->dev, "Fatal error during modeset init\n");

	/* Call ACPI methods: require modeset init
	 * but failure is not fatal
	 */
	if (!r) {
		acpi_status = radeon_acpi_init(rdev);
		if (acpi_status)
			DRM_DEBUG("Error during ACPI methods call\n");
	}

#ifdef notyet
	radeon_kfd_device_probe(rdev);
	radeon_kfd_device_init(rdev);
#endif

	if (radeon_is_px(rdev->ddev)) {
		pm_runtime_use_autosuspend(dev->dev);
		pm_runtime_set_autosuspend_delay(dev->dev, 5000);
		pm_runtime_set_active(dev->dev);
		pm_runtime_allow(dev->dev);
		pm_runtime_mark_last_busy(dev->dev);
		pm_runtime_put_autosuspend(dev->dev);
	}

{
	struct drm_fb_helper *fb_helper = (void *)rdev->mode_info.rfbdev;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &rdev->ro;

	task_set(&rdev->switchtask, radeondrm_doswitch, ri);

	if (ri->ri_bits == NULL)
		return;

#ifdef __sparc64__
	fbwscons_setcolormap(&rdev->sf, radeondrm_setcolor);
#endif
	drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper);

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
	pci_disable_legacy_vga(&rdev->self);

	printf("%s: %dx%d, %dbpp\n", rdev->self.dv_xname,
	    ri->ri_width, ri->ri_height, ri->ri_depth);

	config_found_sm(&rdev->self, &aa, wsemuldisplaydevprint,
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
		radeon_suspend_kms(rdev->ddev, true, true);
		break;
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		break;
	case DVACT_WAKEUP:
		radeon_resume_kms(rdev->ddev, true, true);
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
static void radeon_set_filp_rights(struct drm_device *dev,
				   struct drm_file **owner,
				   struct drm_file *applier,
				   uint32_t *value)
{
	struct radeon_device *rdev = dev->dev_private;

	mutex_lock(&rdev->gem.mutex);
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
	mutex_unlock(&rdev->gem.mutex);
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
static int radeon_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct radeon_device *rdev = dev->dev_private;
	struct drm_radeon_info *info = data;
	struct radeon_mode_info *minfo = &rdev->mode_info;
	uint32_t *value, value_tmp, *value_ptr, value_size;
	uint64_t value64;
	struct drm_crtc *crtc;
	int i, found;

	value_ptr = (uint32_t *)((unsigned long)info->value);
	value = &value_tmp;
	value_size = sizeof(uint32_t);

	switch (info->request) {
	case RADEON_INFO_DEVICE_ID:
		*value = dev->pdev->device;
		break;
	case RADEON_INFO_NUM_GB_PIPES:
		*value = rdev->num_gb_pipes;
		break;
	case RADEON_INFO_NUM_Z_PIPES:
		*value = rdev->num_z_pipes;
		break;
	case RADEON_INFO_ACCEL_WORKING:
		/* xf86-video-ati 6.13.0 relies on this being false for evergreen */
		if ((rdev->family >= CHIP_CEDAR) && (rdev->family <= CHIP_HEMLOCK))
			*value = false;
		else
			*value = rdev->accel_working;
		break;
	case RADEON_INFO_CRTC_FROM_ID:
		if (copy_from_user(value, value_ptr, sizeof(uint32_t))) {
			DRM_ERROR("copy_from_user %s:%u\n", __func__, __LINE__);
			return -EFAULT;
		}
		for (i = 0, found = 0; i < rdev->num_crtc; i++) {
			crtc = (struct drm_crtc *)minfo->crtcs[i];
			if (crtc && crtc->base.id == *value) {
				struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
				*value = radeon_crtc->crtc_id;
				found = 1;
				break;
			}
		}
		if (!found) {
			DRM_DEBUG_KMS("unknown crtc id %d\n", *value);
			return -EINVAL;
		}
		break;
	case RADEON_INFO_ACCEL_WORKING2:
		if (rdev->family == CHIP_HAWAII) {
			if (rdev->accel_working) {
				if (rdev->new_fw)
					*value = 3;
				else
					*value = 2;
			} else {
				*value = 0;
			}
		} else {
			*value = rdev->accel_working;
		}
		break;
	case RADEON_INFO_TILING_CONFIG:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.tile_config;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.tile_config;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.tile_config;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.tile_config;
		else if (rdev->family >= CHIP_RV770)
			*value = rdev->config.rv770.tile_config;
		else if (rdev->family >= CHIP_R600)
			*value = rdev->config.r600.tile_config;
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
		if (copy_from_user(value, value_ptr, sizeof(uint32_t))) {
			DRM_ERROR("copy_from_user %s:%u\n", __func__, __LINE__);
			return -EFAULT;
		}
		if (*value >= 2) {
			DRM_DEBUG_KMS("WANT_HYPERZ: invalid value %d\n", *value);
			return -EINVAL;
		}
		radeon_set_filp_rights(dev, &rdev->hyperz_filp, filp, value);
		break;
	case RADEON_INFO_WANT_CMASK:
		/* The same logic as Hyper-Z. */
		if (copy_from_user(value, value_ptr, sizeof(uint32_t))) {
			DRM_ERROR("copy_from_user %s:%u\n", __func__, __LINE__);
			return -EFAULT;
		}
		if (*value >= 2) {
			DRM_DEBUG_KMS("WANT_CMASK: invalid value %d\n", *value);
			return -EINVAL;
		}
		radeon_set_filp_rights(dev, &rdev->cmask_filp, filp, value);
		break;
	case RADEON_INFO_CLOCK_CRYSTAL_FREQ:
		/* return clock value in KHz */
		if (rdev->asic->get_xclk)
			*value = radeon_get_xclk(rdev) * 10;
		else
			*value = rdev->clock.spll.reference_freq * 10;
		break;
	case RADEON_INFO_NUM_BACKENDS:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.max_backends_per_se *
				rdev->config.cik.max_shader_engines;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.max_backends_per_se *
				rdev->config.si.max_shader_engines;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.max_backends_per_se *
				rdev->config.cayman.max_shader_engines;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.max_backends;
		else if (rdev->family >= CHIP_RV770)
			*value = rdev->config.rv770.max_backends;
		else if (rdev->family >= CHIP_R600)
			*value = rdev->config.r600.max_backends;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_NUM_TILE_PIPES:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.max_tile_pipes;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.max_tile_pipes;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.max_tile_pipes;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.max_tile_pipes;
		else if (rdev->family >= CHIP_RV770)
			*value = rdev->config.rv770.max_tile_pipes;
		else if (rdev->family >= CHIP_R600)
			*value = rdev->config.r600.max_tile_pipes;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_FUSION_GART_WORKING:
		*value = 1;
		break;
	case RADEON_INFO_BACKEND_MAP:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.backend_map;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.backend_map;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.backend_map;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.backend_map;
		else if (rdev->family >= CHIP_RV770)
			*value = rdev->config.rv770.backend_map;
		else if (rdev->family >= CHIP_R600)
			*value = rdev->config.r600.backend_map;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_VA_START:
		/* this is where we report if vm is supported or not */
		if (rdev->family < CHIP_CAYMAN)
			return -EINVAL;
		*value = RADEON_VA_RESERVED_SIZE;
		break;
	case RADEON_INFO_IB_VM_MAX_SIZE:
		/* this is where we report if vm is supported or not */
		if (rdev->family < CHIP_CAYMAN)
			return -EINVAL;
		*value = RADEON_IB_VM_MAX_SIZE;
		break;
	case RADEON_INFO_MAX_PIPES:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.max_cu_per_sh;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.max_cu_per_sh;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.max_pipes_per_simd;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.max_pipes;
		else if (rdev->family >= CHIP_RV770)
			*value = rdev->config.rv770.max_pipes;
		else if (rdev->family >= CHIP_R600)
			*value = rdev->config.r600.max_pipes;
		else {
			return -EINVAL;
		}
		break;
	case RADEON_INFO_TIMESTAMP:
		if (rdev->family < CHIP_R600) {
			DRM_DEBUG_KMS("timestamp is r6xx+ only!\n");
			return -EINVAL;
		}
		value = (uint32_t*)&value64;
		value_size = sizeof(uint64_t);
		value64 = radeon_get_gpu_clock_counter(rdev);
		break;
	case RADEON_INFO_MAX_SE:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.max_shader_engines;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.max_shader_engines;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.max_shader_engines;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.num_ses;
		else
			*value = 1;
		break;
	case RADEON_INFO_MAX_SH_PER_SE:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.max_sh_per_se;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.max_sh_per_se;
		else
			return -EINVAL;
		break;
	case RADEON_INFO_FASTFB_WORKING:
		*value = rdev->fastfb_working;
		break;
	case RADEON_INFO_RING_WORKING:
		if (copy_from_user(value, value_ptr, sizeof(uint32_t))) {
			DRM_ERROR("copy_from_user %s:%u\n", __func__, __LINE__);
			return -EFAULT;
		}
		switch (*value) {
		case RADEON_CS_RING_GFX:
		case RADEON_CS_RING_COMPUTE:
			*value = rdev->ring[RADEON_RING_TYPE_GFX_INDEX].ready;
			break;
		case RADEON_CS_RING_DMA:
			*value = rdev->ring[R600_RING_TYPE_DMA_INDEX].ready;
			*value |= rdev->ring[CAYMAN_RING_TYPE_DMA1_INDEX].ready;
			break;
		case RADEON_CS_RING_UVD:
			*value = rdev->ring[R600_RING_TYPE_UVD_INDEX].ready;
			break;
		case RADEON_CS_RING_VCE:
			*value = rdev->ring[TN_RING_TYPE_VCE1_INDEX].ready;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RADEON_INFO_SI_TILE_MODE_ARRAY:
		if (rdev->family >= CHIP_BONAIRE) {
			value = rdev->config.cik.tile_mode_array;
			value_size = sizeof(uint32_t)*32;
		} else if (rdev->family >= CHIP_TAHITI) {
			value = rdev->config.si.tile_mode_array;
			value_size = sizeof(uint32_t)*32;
		} else {
			DRM_DEBUG_KMS("tile mode array is si+ only!\n");
			return -EINVAL;
		}
		break;
	case RADEON_INFO_CIK_MACROTILE_MODE_ARRAY:
		if (rdev->family >= CHIP_BONAIRE) {
			value = rdev->config.cik.macrotile_mode_array;
			value_size = sizeof(uint32_t)*16;
		} else {
			DRM_DEBUG_KMS("macrotile mode array is cik+ only!\n");
			return -EINVAL;
		}
		break;
	case RADEON_INFO_SI_CP_DMA_COMPUTE:
		*value = 1;
		break;
	case RADEON_INFO_SI_BACKEND_ENABLED_MASK:
		if (rdev->family >= CHIP_BONAIRE) {
			*value = rdev->config.cik.backend_enable_mask;
		} else if (rdev->family >= CHIP_TAHITI) {
			*value = rdev->config.si.backend_enable_mask;
		} else {
			DRM_DEBUG_KMS("BACKEND_ENABLED_MASK is si+ only!\n");
		}
		break;
	case RADEON_INFO_MAX_SCLK:
		if ((rdev->pm.pm_method == PM_METHOD_DPM) &&
		    rdev->pm.dpm_enabled)
			*value = rdev->pm.dpm.dyn_state.max_clock_voltage_on_ac.sclk * 10;
		else
			*value = rdev->pm.default_sclk * 10;
		break;
	case RADEON_INFO_VCE_FW_VERSION:
		*value = rdev->vce.fw_version;
		break;
	case RADEON_INFO_VCE_FB_VERSION:
		*value = rdev->vce.fb_version;
		break;
	case RADEON_INFO_NUM_BYTES_MOVED:
		value = (uint32_t*)&value64;
		value_size = sizeof(uint64_t);
		value64 = atomic64_read(&rdev->num_bytes_moved);
		break;
	case RADEON_INFO_VRAM_USAGE:
		value = (uint32_t*)&value64;
		value_size = sizeof(uint64_t);
		value64 = atomic64_read(&rdev->vram_usage);
		break;
	case RADEON_INFO_GTT_USAGE:
		value = (uint32_t*)&value64;
		value_size = sizeof(uint64_t);
		value64 = atomic64_read(&rdev->gtt_usage);
		break;
	case RADEON_INFO_ACTIVE_CU_COUNT:
		if (rdev->family >= CHIP_BONAIRE)
			*value = rdev->config.cik.active_cus;
		else if (rdev->family >= CHIP_TAHITI)
			*value = rdev->config.si.active_cus;
		else if (rdev->family >= CHIP_CAYMAN)
			*value = rdev->config.cayman.active_simds;
		else if (rdev->family >= CHIP_CEDAR)
			*value = rdev->config.evergreen.active_simds;
		else if (rdev->family >= CHIP_RV770)
			*value = rdev->config.rv770.active_simds;
		else if (rdev->family >= CHIP_R600)
			*value = rdev->config.r600.active_simds;
		else
			*value = 1;
		break;
	case RADEON_INFO_CURRENT_GPU_TEMP:
		/* get temperature in millidegrees C */
		if (rdev->asic->pm.get_temperature)
			*value = radeon_get_temperature(rdev);
		else
			*value = 0;
		break;
	case RADEON_INFO_CURRENT_GPU_SCLK:
		/* get sclk in Mhz */
		if (rdev->pm.dpm_enabled)
			*value = radeon_dpm_get_current_sclk(rdev) / 100;
		else
			*value = rdev->pm.current_sclk / 100;
		break;
	case RADEON_INFO_CURRENT_GPU_MCLK:
		/* get mclk in Mhz */
		if (rdev->pm.dpm_enabled)
			*value = radeon_dpm_get_current_mclk(rdev) / 100;
		else
			*value = rdev->pm.current_mclk / 100;
		break;
	case RADEON_INFO_READ_REG:
		if (copy_from_user(value, value_ptr, sizeof(uint32_t))) {
			DRM_ERROR("copy_from_user %s:%u\n", __func__, __LINE__);
			return -EFAULT;
		}
		if (radeon_get_allowed_info_register(rdev, *value, value))
			return -EINVAL;
		break;
	case RADEON_INFO_VA_UNMAP_WORKING:
		*value = true;
		break;
	case RADEON_INFO_GPU_RESET_COUNTER:
		*value = atomic_read(&rdev->gpu_reset_counter);
		break;
	default:
		DRM_DEBUG_KMS("Invalid request %d\n", info->request);
		return -EINVAL;
	}
	if (copy_to_user(value_ptr, (char*)value, value_size)) {
		DRM_ERROR("copy_to_user %s:%u\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
}


/*
 * Outdated mess for old drm with Xorg being in charge (void function now).
 */
/**
 * radeon_driver_lastclose_kms - drm callback for last close
 *
 * @dev: drm dev pointer
 *
 * Switch vga_switcheroo state after last close (all asics).
 */
void radeon_driver_lastclose_kms(struct drm_device *dev)
{
	struct radeon_device *rdev = dev->dev_private;

#ifdef __sparc64__
	fbwscons_setcolormap(&rdev->sf, radeondrm_setcolor);
#endif
	if (rdev->mode_info.mode_config_initialized)
		radeon_fbdev_restore_mode(rdev);
	vga_switcheroo_process_delayed_switch();
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
	int r;

	file_priv->driver_priv = NULL;

	r = pm_runtime_get_sync(dev->dev);
	if (r < 0)
		return r;

	/* new gpu have virtual address space support */
	if (rdev->family >= CHIP_CAYMAN) {
		struct radeon_fpriv *fpriv;
		struct radeon_vm *vm;
		int r;

		fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
		if (unlikely(!fpriv)) {
			return -ENOMEM;
		}

		if (rdev->accel_working) {
			vm = &fpriv->vm;
			r = radeon_vm_init(rdev, vm);
			if (r) {
				kfree(fpriv);
				return r;
			}

			r = radeon_bo_reserve(rdev->ring_tmp_bo.bo, false);
			if (r) {
				radeon_vm_fini(rdev, vm);
				kfree(fpriv);
				return r;
			}

			/* map the ib pool buffer read only into
			 * virtual address space */
			vm->ib_bo_va = radeon_vm_bo_add(rdev, vm,
							rdev->ring_tmp_bo.bo);
			r = radeon_vm_bo_set_addr(rdev, vm->ib_bo_va,
						  RADEON_VA_IB_OFFSET,
						  RADEON_VM_PAGE_READABLE |
						  RADEON_VM_PAGE_SNOOPED);
			if (r) {
				radeon_vm_fini(rdev, vm);
				kfree(fpriv);
				return r;
			}
		}
		file_priv->driver_priv = fpriv;
	}

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);
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
		struct radeon_vm *vm = &fpriv->vm;
		int r;

		if (rdev->accel_working) {
			r = radeon_bo_reserve(rdev->ring_tmp_bo.bo, false);
			if (!r) {
				if (vm->ib_bo_va)
					radeon_vm_bo_rmv(rdev, vm->ib_bo_va);
				radeon_bo_unreserve(rdev->ring_tmp_bo.bo);
			}
			radeon_vm_fini(rdev, vm);
		}

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

	mutex_lock(&rdev->gem.mutex);
	if (rdev->hyperz_filp == file_priv)
		rdev->hyperz_filp = NULL;
	if (rdev->cmask_filp == file_priv)
		rdev->cmask_filp = NULL;
	mutex_unlock(&rdev->gem.mutex);

	radeon_uvd_free_handles(rdev, file_priv);
	radeon_vce_free_handles(rdev, file_priv);
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
	int vpos, hpos, stat;
	u32 count;
	struct radeon_device *rdev = dev->dev_private;

	if (crtc < 0 || crtc >= rdev->num_crtc) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	/* The hw increments its frame counter at start of vsync, not at start
	 * of vblank, as is required by DRM core vblank counter handling.
	 * Cook the hw count here to make it appear to the caller as if it
	 * incremented at start of vblank. We measure distance to start of
	 * vblank in vpos. vpos therefore will be >= 0 between start of vblank
	 * and start of vsync, so vpos >= 0 means to bump the hw frame counter
	 * result by 1 to give the proper appearance to caller.
	 */
	if (rdev->mode_info.crtcs[crtc]) {
		/* Repeat readout if needed to provide stable result if
		 * we cross start of vsync during the queries.
		 */
		do {
			count = radeon_get_vblank_counter(rdev, crtc);
			/* Ask radeon_get_crtc_scanoutpos to return vpos as
			 * distance to start of vblank, instead of regular
			 * vertical scanout pos.
			 */
			stat = radeon_get_crtc_scanoutpos(
				dev, crtc, GET_DISTANCE_TO_VBLANKSTART,
				&vpos, &hpos, NULL, NULL,
				&rdev->mode_info.crtcs[crtc]->base.hwmode);
		} while (count != radeon_get_vblank_counter(rdev, crtc));

		if (((stat & (DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_ACCURATE)) !=
		    (DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_ACCURATE))) {
			DRM_DEBUG_VBL("Query failed! stat %d\n", stat);
		}
		else {
			DRM_DEBUG_VBL("crtc %d: dist from vblank start %d\n",
				      crtc, vpos);

			/* Bump counter if we are at >= leading edge of vblank,
			 * but before vsync where vpos would turn negative and
			 * the hw counter really increments.
			 */
			if (vpos >= 0)
				count++;
		}
	}
	else {
	    /* Fallback to use value as is. */
	    count = radeon_get_vblank_counter(rdev, crtc);
	    DRM_DEBUG_VBL("NULL mode info! Returned count may be wrong.\n");
	}

	return count;
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
	unsigned long irqflags;
	int r;

	if (crtc < 0 || crtc >= rdev->num_crtc) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	rdev->irq.crtc_vblank_int[crtc] = true;
	r = radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
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
	unsigned long irqflags;

	if (crtc < 0 || crtc >= rdev->num_crtc) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return;
	}

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	rdev->irq.crtc_vblank_int[crtc] = false;
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
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
	if (!drmcrtc)
		return -EINVAL;

	/* Helper routine in DRM core does all the work: */
	return drm_calc_vbltimestamp_from_scanoutpos(dev, crtc, max_error,
						     vblank_time, flags,
						     &drmcrtc->hwmode);
}

const struct drm_ioctl_desc radeon_ioctls_kms[] = {
	DRM_IOCTL_DEF_DRV(RADEON_CP_INIT, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_START, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_STOP, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_RESET, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_IDLE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CP_RESUME, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_RESET, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FULLSCREEN, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SWAP, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CLEAR, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_VERTEX, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INDICES, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_TEXTURE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_STIPPLE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INDIRECT, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_VERTEX2, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CMDBUF, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_GETPARAM, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FLIP, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_ALLOC, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FREE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INIT_HEAP, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_IRQ_EMIT, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_IRQ_WAIT, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SETPARAM, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SURF_ALLOC, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SURF_FREE, drm_invalid_op, DRM_AUTH),
	/* KMS */
	DRM_IOCTL_DEF_DRV(RADEON_GEM_INFO, radeon_gem_info_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_CREATE, radeon_gem_create_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_MMAP, radeon_gem_mmap_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_SET_DOMAIN, radeon_gem_set_domain_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_PREAD, radeon_gem_pread_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_PWRITE, radeon_gem_pwrite_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_WAIT_IDLE, radeon_gem_wait_idle_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_CS, radeon_cs_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_INFO, radeon_info_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_SET_TILING, radeon_gem_set_tiling_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_GET_TILING, radeon_gem_get_tiling_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_BUSY, radeon_gem_busy_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_VA, radeon_gem_va_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_OP, radeon_gem_op_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_USERPTR, radeon_gem_userptr_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
};
int radeon_max_kms_ioctl = ARRAY_SIZE(radeon_ioctls_kms);
