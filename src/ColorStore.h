/*
 * All code here is trimmed down/modified code from here:
 * https://github.com/cdyk/rvmparser
 */

#pragma once
#include <unordered_map>
class ColorStore
{

public:
    std::unordered_map<uint32_t, uint32_t> p_id_hex;

    ColorStore()
    {
        p_id_hex.insert_or_assign(1, 0x000000);
        p_id_hex.insert_or_assign(2, 0xcc0000);
        p_id_hex.insert_or_assign(3, 0xed9900);
        p_id_hex.insert_or_assign(4, 0xcccc00);
        p_id_hex.insert_or_assign(5, 0x00cc00);
        p_id_hex.insert_or_assign(6, 0x00eded);
        p_id_hex.insert_or_assign(7, 0x0000cc);
        p_id_hex.insert_or_assign(8, 0xdd00dd);
        p_id_hex.insert_or_assign(9, 0xcc2b2b);
        p_id_hex.insert_or_assign(10, 0xffffff);
        p_id_hex.insert_or_assign(11, 0xf97f70);
        p_id_hex.insert_or_assign(12, 0xbfbfbf);
        p_id_hex.insert_or_assign(13, 0xa8a8a8);
        p_id_hex.insert_or_assign(14, 0x8c668c);
        p_id_hex.insert_or_assign(15, 0xf4f4f4);
        p_id_hex.insert_or_assign(16, 0x8e236b);
        p_id_hex.insert_or_assign(17, 0x00ff7f);
        p_id_hex.insert_or_assign(18, 0xf4ddb2);
        p_id_hex.insert_or_assign(19, 0xedc933);
        p_id_hex.insert_or_assign(20, 0x4775ff);
        p_id_hex.insert_or_assign(21, 0xede8aa);
        p_id_hex.insert_or_assign(22, 0xed1189);
        p_id_hex.insert_or_assign(23, 0x238e23);
        p_id_hex.insert_or_assign(24, 0xffa500);
        p_id_hex.insert_or_assign(25, 0xedede0);
        p_id_hex.insert_or_assign(26, 0xed7521);
        p_id_hex.insert_or_assign(27, 0x4782b5);
        p_id_hex.insert_or_assign(28, 0xffffff);
        p_id_hex.insert_or_assign(29, 0x2d2d4f);
        p_id_hex.insert_or_assign(30, 0x00007f);
        p_id_hex.insert_or_assign(31, 0xcc919e);
        p_id_hex.insert_or_assign(32, 0xcc5b44);
        p_id_hex.insert_or_assign(33, 0x000000);
        p_id_hex.insert_or_assign(34, 0xcc0000);
        p_id_hex.insert_or_assign(35, 0xed9900);
        p_id_hex.insert_or_assign(36, 0xcccc00);
        p_id_hex.insert_or_assign(37, 0x00cc00);
        p_id_hex.insert_or_assign(38, 0x00eded);
        p_id_hex.insert_or_assign(39, 0x0000cc);
        p_id_hex.insert_or_assign(40, 0xdd00dd);
        p_id_hex.insert_or_assign(41, 0xcc2b2b);
        p_id_hex.insert_or_assign(42, 0xffffff);
        p_id_hex.insert_or_assign(43, 0xf97f70);
        p_id_hex.insert_or_assign(44, 0xbfbfbf);
        p_id_hex.insert_or_assign(45, 0xa8a8a8);
        p_id_hex.insert_or_assign(46, 0x8c668c);
        p_id_hex.insert_or_assign(47, 0xf4f4f4);
        p_id_hex.insert_or_assign(48, 0x8e236b);
        p_id_hex.insert_or_assign(49, 0x00ff7f);
        p_id_hex.insert_or_assign(50, 0xf4ddb2);
        p_id_hex.insert_or_assign(51, 0xedc933);
        p_id_hex.insert_or_assign(52, 0x4775ff);
        p_id_hex.insert_or_assign(53, 0xede8aa);
        p_id_hex.insert_or_assign(54, 0xed1189);
        p_id_hex.insert_or_assign(55, 0x238e23);
        p_id_hex.insert_or_assign(56, 0xffa500);
        p_id_hex.insert_or_assign(57, 0xedede0);
        p_id_hex.insert_or_assign(58, 0xed7521);
        p_id_hex.insert_or_assign(59, 0x4782b5);
        p_id_hex.insert_or_assign(60, 0xffffff);
        p_id_hex.insert_or_assign(61, 0x2d2d4f);
        p_id_hex.insert_or_assign(62, 0x00007f);
        p_id_hex.insert_or_assign(63, 0xcc919e);
        p_id_hex.insert_or_assign(64, 0xcc5b44);
        p_id_hex.insert_or_assign(206, 0x000000);
        p_id_hex.insert_or_assign(207, 0xffffff);
        p_id_hex.insert_or_assign(208, 0xf4f4f4);
        p_id_hex.insert_or_assign(209, 0xedede0);
        p_id_hex.insert_or_assign(210, 0xa8a8a8);
        p_id_hex.insert_or_assign(211, 0xbfbfbf);
        p_id_hex.insert_or_assign(212, 0x518c8c);
        p_id_hex.insert_or_assign(213, 0x2d4f4f);
        p_id_hex.insert_or_assign(214, 0xcc0000);
        p_id_hex.insert_or_assign(215, 0xff0000);
        p_id_hex.insert_or_assign(216, 0xcc5b44);
        p_id_hex.insert_or_assign(217, 0xff6347);
        p_id_hex.insert_or_assign(218, 0x8c668c);
        p_id_hex.insert_or_assign(219, 0xed1189);
        p_id_hex.insert_or_assign(220, 0xcc919e);
        p_id_hex.insert_or_assign(221, 0xf97f70);
        p_id_hex.insert_or_assign(222, 0xed9900);
        p_id_hex.insert_or_assign(223, 0xffa500);
        p_id_hex.insert_or_assign(224, 0xff7f00);
        p_id_hex.insert_or_assign(225, 0x8e236b);
        p_id_hex.insert_or_assign(226, 0xcccc00);
        p_id_hex.insert_or_assign(227, 0xedc933);
        p_id_hex.insert_or_assign(228, 0xededd1);
        p_id_hex.insert_or_assign(229, 0xede8aa);
        p_id_hex.insert_or_assign(230, 0x99cc33);
        p_id_hex.insert_or_assign(231, 0x00ff7f);
        p_id_hex.insert_or_assign(232, 0x00cc00);
        p_id_hex.insert_or_assign(233, 0x238e23);
        p_id_hex.insert_or_assign(234, 0x2d4f2d);
        p_id_hex.insert_or_assign(235, 0x00eded);
        p_id_hex.insert_or_assign(236, 0x00bfcc);
        p_id_hex.insert_or_assign(237, 0x75edc6);
        p_id_hex.insert_or_assign(238, 0x0000cc);
        p_id_hex.insert_or_assign(239, 0x4775ff);
        p_id_hex.insert_or_assign(240, 0x00007f);
        p_id_hex.insert_or_assign(241, 0xafe0e5);
        p_id_hex.insert_or_assign(242, 0x2d2d4f);
        p_id_hex.insert_or_assign(243, 0x4782b5);
        p_id_hex.insert_or_assign(244, 0x330066);
        p_id_hex.insert_or_assign(245, 0x660099);
        p_id_hex.insert_or_assign(246, 0xed82ed);
        p_id_hex.insert_or_assign(247, 0xdd00dd);
        p_id_hex.insert_or_assign(248, 0xf4f4db);
        p_id_hex.insert_or_assign(249, 0xf4ddb2);
        p_id_hex.insert_or_assign(250, 0xdb9370);
        p_id_hex.insert_or_assign(251, 0xf4a55e);
        p_id_hex.insert_or_assign(252, 0xcc2b2b);
        p_id_hex.insert_or_assign(253, 0x9e9e5e);
        p_id_hex.insert_or_assign(254, 0xed7521);
        p_id_hex.insert_or_assign(255, 0x8c4414);
    }
};