// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define dout	printf
#include "SparrowTingHuLogic.h"

#define MAX_CARDS	14

const char * const VALUE_NAMES[] = { "一", "二", "三", "四", "五", "六", "七", "八", "九" };
const char * const FENG_NAMES[]  = { "东风", "南风", "西风", "北风", "红中", "发财", "白板" };
const char * const COLOR_NAMES[] = { "饼", "萬", "条" };

#define MAX_VALUE	9

const char * const CardName(uint8_t card_data)
{
	int color = (card_data & 0xF0) >> 4;
	int value = card_data & 0x0F;
	if (color == 3) return FENG_NAMES[value-1];

	static char name[8] = "";
	sprintf(name, "%s%s", VALUE_NAMES[value-1], COLOR_NAMES[color]);
	return name;
}

int ipow_of_10(int n)
{
	if (n < 0 || n > 9) return 0;

	static int pow_v[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

	return pow_v[n];
}

void CreateHuSegmentXiang(uint8_t pool[MAX_VALUE], 
						  int maxLevel, 
						  int currentLevel, 
						  int beginValueKe, 
						  int beginValueShun, 
						  int upperSignature, 
						  uint8_t jiang, 
						  HuSegmentMap & hst)
{
	if (currentLevel > maxLevel) return;

	uint8_t is_jiang = jiang > 0 ? 1 : 0;
	uint8_t is_258 = jiang == 2 || jiang == 5 || jiang == 8;
	int signature = 0;

	// 刻子
	for (int i = beginValueKe; i <= MAX_VALUE; i++)
	{
		if (pool[i - 1] > 3)
		{
			signature = upperSignature + ipow_of_10(i - 1) * 3;
			HU_SEGMENT_INFO inf = { 0, is_jiang, is_258 };
			hst[signature] = inf; // 非风牌
			hst[signature + SparrowTingHuLogic::FENG_SIGNATURE_CAP] = inf; // 风牌专用，风牌无顺子

			pool[i-1] -= 3;
			CreateHuSegmentXiang(pool, maxLevel, currentLevel + 1, i + 1, 1, signature, jiang, hst);
			pool[i-1] += 3;
		}
	}

	// 顺子
	for (int i = beginValueShun; i <= MAX_VALUE - 2; i++)
	{
		if (pool[i-1] > 1 && pool[i] > 1 && pool[i+1] > 1)
		{
			signature = upperSignature + ipow_of_10(i - 1) + ipow_of_10(i) + ipow_of_10(i + 1);
			HU_SEGMENT_INFO inf = {1, is_jiang, is_258 };
			hst[signature] = inf;

			pool[i-1]--;
			pool[i]--;
			pool[i+1]--;
			CreateHuSegmentXiang(pool, maxLevel, currentLevel + 1, MAX_VALUE + 1, i, signature, jiang, hst);
			pool[i-1]++;
			pool[i]++;
			pool[i+1]++;
		}
	}
}

const char * const TranslateHuSignature(int signature)
{
	static const char * const NAME_MAP[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九" };
	static char buffer[1024] = "";

	int n = 0;
	memset(buffer, sizeof(buffer), 0);
	for (int i=0; i<MAX_VALUE; i++)
	{
		int p = (signature % ipow_of_10(i+1)) / ipow_of_10(i);
		int m = 0;
		for (int j = 0; j < p; j++)
		{
			m = sprintf(buffer + n, "%s", NAME_MAP[i]);
			n += m;
		}
		if (m > 0) n += sprintf(buffer + n, " ");
	}

	return buffer;
}

void DumpHuSegmentTable(const HuSegmentMap & hst)
{
	dout("[特征码]    [麻将序列]\n");
	dout("========================================================================\n");

	for (HuSegmentMap::const_iterator itr = hst.begin(); itr != hst.end(); itr++)
	{
		int signature = itr->first;
		dout("%010d : %s\n", signature, TranslateHuSignature(signature));
	}

	dout("总共 %d 个组合。\n", hst.size());
}

void CreateSegmentByCutCard(const HuSegmentMap & hst, HuSegmentMap & new_hst)
{
	for (HuSegmentMap::const_iterator itr = hst.begin(); itr != hst.end(); itr++)
	{
		int signature = itr->first;
		HU_SEGMENT_INFO hsi = itr->second;

		for (int i=0; i<MAX_VALUE; i++)
		{
			int wei_n = (signature % ipow_of_10(i+1)) / ipow_of_10(i);
			if (wei_n > 0)
			{
				int signature_magic = signature - ipow_of_10(i);
				if (signature_magic == 0)
				{
					// 说明此片段只有一张麻将，就不必继续下去了，不能再扣除混子了。
					break;
				}

				// 无混子、非风牌片断  XXX 和 XYZ 扣到两张混子时，会出现重复片断 X
				// 此时排后面的会片断覆盖顺子片断的map项，不过没有关系

				hsi.magic_count = itr->second.magic_count + 1;
				new_hst[signature_magic] = hsi;
				//dout("%09d -> magicfied: %09d\n", signature, signature_magic);
			}
		}
	}
}

void CreateHuSegmentTable(HuSegmentMap & hst_no_magic, HuSegmentMap & hst_1magic, HuSegmentMap & hst_2magic, HuSegmentMap & hst_3magic)
{
	uint8_t pool[MAX_VALUE];
	for (int i=0; i<MAX_VALUE; ++i)
	{
		pool[i] = 4;
	}

	// A. 无混子情况
	// 1. 含将牌情况
	for (int i = 1; i <= MAX_VALUE; ++i)
	{
		int signature = ipow_of_10(i - 1) * 2;
		int is_258 = (i == 2 || i == 5 || i == 8);
		HU_SEGMENT_INFO inf = { 0, 1, is_258 };
		hst_no_magic[signature] = inf;
		hst_no_magic[signature + SparrowTingHuLogic::FENG_SIGNATURE_CAP] = inf; // 风牌专用

		pool[i-1] -= 2;
		CreateHuSegmentXiang(pool, 4, 1, 1, 1, signature, i, hst_no_magic);
		pool[i-1] += 2;
	}

	// 2. 不含将牌情况
	CreateHuSegmentXiang(pool, 4, 1, 1, 1, 0, 0, hst_no_magic);

	// B. 混子情况
	// 将无混子情况的每个方案的每种牌，数量依次扣除一~三张，得到混子情况
	// 由于4混子直接胡牌，因此混子牌数量只计算到3张。
	
	// WARNING: 对于某花色片段剩单张的情况，既有可能是将对扣一张混子，也可能是刻子或顺子扣两张混子，特征码一致，因此各张混子片段表分开保存
	
	// B.1 一张混子的情况
	CreateSegmentByCutCard(hst_no_magic, hst_1magic);
	//hst_magic.insert(hst_1magic.begin(), hst_1magic.end());

	// B.2 两张混子的情况
	CreateSegmentByCutCard(hst_1magic, hst_2magic);
	//hst_magic.insert(hst_2magic.begin(), hst_2magic.end());

	// B.3 三张混子的情况
	CreateSegmentByCutCard(hst_2magic, hst_3magic);
	//hst_magic.insert(hst_3magic.begin(), hst_3magic.end());
}

void DumpHuSegmentTableAsSourceCode(FILE * f, const HuSegmentMap & hst, int maxItemCount = -1)
{
	int limit = maxItemCount < 0 ? hst.size() : maxItemCount;

	int n = 0;
	for (HuSegmentMap::const_iterator itr = hst.begin(); itr != hst.end(); itr++)
	{
		if (n >= limit) break;

		fprintf(f, 
			"%d, %d, %d, %d, %d\n", 
			itr->first,
			(int)itr->second.has_shunzi, 
			(int)itr->second.has_jiang, 
			(int)itr->second.jiang_258, 
			(int)itr->second.magic_count
			);

		//fprintf(f, 
		//	"{ HU_SEGMENT_INFO inf = { %d, %d, %d, %d }; hst[%d] = inf; }\n", 
		//	(int)itr->second.has_shunzi, 
		//	(int)itr->second.has_jiang, 
		//	(int)itr->second.jiang_258, 
		//	(int)itr->second.magic_count,
		//	itr->first
		//	);
	}
}

void BuildAndDumpHuSegmentInfoTable()
{
	HuSegmentMap hst_no_magic;
	HuSegmentMap hst_1magic;
	HuSegmentMap hst_2magic;
	HuSegmentMap hst_3magic;

	DWORD ms_begin = GetTickCount();
	CreateHuSegmentTable(hst_no_magic, hst_1magic, hst_2magic, hst_3magic);
	DWORD ms_end = GetTickCount();

	// DumpHuSegmentTable(hst_no_magic);
	dout("总共 %d 项无混子方案。\n", hst_no_magic.size());
	dout("总共 %d 项一混子方案。\n", hst_1magic.size());
	dout("总共 %d 项双混子方案。\n", hst_2magic.size());
	dout("总共 %d 项三混子方案。\n", hst_3magic.size());
	dout("构造总时间：%.3f 秒。", (ms_end-ms_begin)/1000.0);

	FILE * f = fopen("hst.map", "w");
	DumpHuSegmentTableAsSourceCode(f, hst_no_magic);
	fclose(f);
	f = fopen("hst-1magic.map", "w");
	DumpHuSegmentTableAsSourceCode(f, hst_1magic);
	fclose(f);
	f = fopen("hst-2magic.map", "w");
	DumpHuSegmentTableAsSourceCode(f, hst_2magic);
	fclose(f);
	f = fopen("hst-3magic.map", "w");
	DumpHuSegmentTableAsSourceCode(f, hst_3magic);
	fclose(f);
}

void DumpTingInfo(const std::map<uint8_t, std::vector<uint8_t>> & tingInfo)
{
	for (std::map<uint8_t, std::vector<uint8_t>>::const_iterator itr = tingInfo.begin(); itr != tingInfo.end(); itr++)
	{
		uint8_t out_card = itr->first;
		const std::vector<uint8_t> & ting_list = itr->second;

		dout("打出 %s，可听 %d 张：", CardName(out_card), ting_list.size());
		for (std::vector<uint8_t>::const_iterator it = ting_list.begin(); it != ting_list.end(); it++)
		{
			dout("%s ", CardName(*it));
		}
		dout("\n");
	}
}
void DumpCardList(uint8_t * card_data_list, int count)
{
	for (int i=0; i<count; i++)
	{
		dout("[%s] ", CardName(card_data_list[i]));
	}
	dout("\n");
}

void TestTingHuLogic()
{
	DWORD ms_begin = GetTickCount();
	SparrowTingHuLogic::Init("hst.map", "hst-1magic.map", "hst-2magic.map", "hst-3magic.map");
	DWORD ms_end = GetTickCount();
	dout("加载总耗时：%.3f 秒。\n", (ms_end-ms_begin)/1000.0);
	
	// 可以打3饼，听4饼
	// 可以打5饼，听3饼、6饼
	// 可以打6饼，听4、7饼
	//uint8_t hand_cards[] = { 0x03, 0x03, 0x05, 0x05, 0x26 };
	uint8_t hand_cards[] = { 0x02, 0x03, 0x05, 0x07, 0x08, 0x09, 0x16, 0x18, 0x21, 0x22, 0x23, 0x32, 0x32, 0x11 };

	uint8_t magic = 0x32;
	bool use_feng = true;
	std::map<uint8_t, std::vector<uint8_t>> result;
	ms_begin = GetTickCount();
	SparrowTingHuLogic & sthl = SparrowTingHuLogic::Instance();
	bool ke_ting = sthl.CalcTingableCardInfo(hand_cards, sizeof(hand_cards)/sizeof(hand_cards[0]), magic, false, use_feng, result);
	ms_end = GetTickCount();

	dout("\n混子：%s\n能胡风牌：%s\n\n手牌：\n", magic ? CardName(magic) : "[无]", use_feng ? "是" : "否");
	DumpCardList(hand_cards, sizeof(hand_cards)/sizeof(hand_cards[0]));
	dout("\n");

	if (ke_ting)
	{
		DumpTingInfo(result);
	}
	else
	{
		dout("失败：这手牌打出任何一张都无法听牌。\n");
	}
	dout("\n");
	dout("判定耗时：%.3f 秒。\n", (ms_end-ms_begin)/1000.0);
}

int _tmain(int argc, _TCHAR* argv[])
{
	//BuildAndDumpHuSegmentInfoTable();
	TestTingHuLogic();

	return 0;
}
