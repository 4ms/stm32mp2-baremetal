#ifndef _GENERIC_PHY_H
#define _GENERIC_PHY_H

/* Minimal PHY stubs — DWC3 core.c references generic_phy but
 * PHY power-up is handled externally in bare-metal. */

struct phy {
	int id;
};

static inline int generic_phy_init(struct phy *phy) { (void)phy; return 0; }
static inline int generic_phy_power_on(struct phy *phy) { (void)phy; return 0; }
static inline int generic_phy_power_off(struct phy *phy) { (void)phy; return 0; }
static inline int generic_phy_exit(struct phy *phy) { (void)phy; return 0; }

#endif /* _GENERIC_PHY_H */
