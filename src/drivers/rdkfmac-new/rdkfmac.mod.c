#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
    .name = KBUILD_MODNAME,
    .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
    .exit = cleanup_module,
#endif
    .arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
    { 0xe7a02573, "ida_alloc_range" },
    { 0xf28cf0ae, "__hw_addr_init" },
    { 0x8cec8f7b, "ieee80211_start_tx_ba_cb_irqsafe" },
    { 0xaee657ee, "__class_create" },
    { 0x8d522714, "__rcu_read_lock" },
    { 0x66b467be, "platform_driver_unregister" },
    { 0x8cf7280c, "device_unregister" },
    { 0x6809f394, "ieee80211_free_hw" },
    { 0x645620c0, "class_destroy" },
    { 0x2d0684a9, "hrtimer_init" },
    { 0x7915eb01, "ieee80211_register_hw" },
    { 0xbdfb6dbb, "__fentry__" },
    { 0x65487097, "__x86_indirect_thunk_rax" },
    { 0xd73ea01, "wiphy_new_nm" },
    { 0x92997ed8, "_printk" },
    { 0xd0da656b, "__stack_chk_fail" },
    { 0xe46021ca, "_raw_spin_unlock_bh" },
    { 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
    { 0x84d70627, "ieee80211_stop_tx_ba_cb_irqsafe" },
    { 0x3fd2f8f8, "cdev_add" },
    { 0x68031039, "init_net" },
    { 0x2469810f, "__rcu_read_unlock" },
    { 0xf2d7865d, "device_create" },
    { 0x4dfa8d4b, "mutex_lock" },
    { 0xffb7c514, "ida_free" },
    { 0x515e298a, "unregister_pernet_device" },
    { 0xcefb0c9f, "__mutex_init" },
    { 0x37befc70, "jiffies_to_msecs" },
    { 0x5927de60, "device_bind_driver" },
    { 0x3c5d543a, "hrtimer_start_range_ns" },
    { 0x5b8239ca, "__x86_return_thunk" },
    { 0x15ba50a6, "jiffies" },
    { 0xe9925143, "__platform_driver_register" },
    { 0x6091b333, "unregister_chrdev_region" },
    { 0xfde69a73, "ieee80211_iterate_active_interfaces_atomic" },
    { 0x3213f038, "mutex_unlock" },
    { 0xc6f46339, "init_timer_key" },
    { 0xdf54a8f7, "netlink_unregister_notifier" },
    { 0xa1e3ebaa, "ieee80211_unregister_hw" },
    { 0x48931e20, "device_destroy" },
    { 0x837b7b09, "__dynamic_pr_debug" },
    { 0x3ac3feba, "rhltable_init" },
    { 0x56470118, "__warn_printk" },
    { 0xc3690fc, "_raw_spin_lock_bh" },
    { 0x46a4b118, "hrtimer_cancel" },
    { 0xfa599bb2, "netlink_register_notifier" },
    { 0x2f26f9f9, "genl_unregister_family" },
    { 0x828e22f4, "hrtimer_forward" },
    { 0x3fd78f3b, "register_chrdev_region" },
    { 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
    { 0xe702e1d8, "device_release_driver" },
    { 0xa846d8e1, "genl_register_family" },
    { 0x9650938d, "wiphy_free" },
    { 0x2d0e82e8, "register_pernet_device" },
    { 0xc4f0da12, "ktime_get_with_offset" },
    { 0xfd205a2d, "cdev_init" },
    { 0x5708ebf2, "cdev_del" },
    { 0x541a6db8, "module_layout" },
};

MODULE_INFO(depends, "mac80211,cfg80211");


MODULE_INFO(srcversion, "5B2105159B7A57CC008C1D8");
