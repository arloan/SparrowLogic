#ifndef HU_SEGMENT_H__
#define HU_SEGMENT_H__

#pragma once

#include <stdint.h>
#include <map>
#include <vector>

// 0 - ��, 1 - ��, 2 - ��, 3 - ��
//#define COLOR_BING	0
//#define COLOR_WAN	1
//#define COLOR_TIAO	2
//#define COLOR_FENG	3

#define MJ_MAX_INDEX	34
#define MJ_FENG_COUNT	7

#ifdef __GNU_C__
#define OPTFUN __attribute__((optimize(2)))
#else
#define OPTFUN
#endif 


// ����ֵ signature:
// �����ڻ�ɫ�����Ϊ1~9(����Ϊ1~7),��������������3,���Ƶ������6,�ȵ�
// ��ôȡһ�����9λ��ʮ���������������е�������,����λ������Ƶ����,λ�ϵ���ֵ�����������Ƶ�����
// ����,�������(ע�Ȿ�����ڵ��ƶ���ͬһ��ɫ��)�е����ǡ����򡢶��������������򡱣�
// ��ȡʮ������001110020���ü��ϣ����У�
// ��λ������Ϊ1�����������˴�������û��һ�������ƣ����Ը�λΪ0��
// ʮλ������Ϊ2�����������˴������У���������������ʮλΪ2���Դ����ơ�
// ע��ʮ��λΪ1ʱ������ר�ñ����Ϊ����ֻ�п��ӣ�û��˳��

// [[ 1��2��3�Ż�������ֿ����棬���·�������ʹ�� <<--
// **Ȼ��**
// ����ĳ��ɫƬ��ʣ���ŵ����������п����ǽ��Կ�һ�Ż��ӣ�Ҳ�����ǿ��ӻ�˳�ӿ����Ż��ӣ�������һ��
// ��ˣ�������ֵ����1��ʮ����λ����Ϊ10λ������10λ����ʮ��λ���۳��Ļ��Ӹ���������
// �����޻�������ֵ��ʮ��ΪΪ0��һ����Ϊ1���Դ����ƣ����3���ӡ�
// -->> ]]

struct HU_SEGMENT_INFO {
	uint8_t has_shunzi;	// ��˳��
	uint8_t has_jiang;	// ������
	uint8_t jiang_258;	// ������258
	uint8_t magic_count;// ��Ҫ����������
};
typedef std::map<int, HU_SEGMENT_INFO> HuSegmentMap;

class SparrowTingHuLogic
{
public:
	static const int FENG_SIGNATURE_CAP = 1000000000;

public:
	static void Init(
		const std::string & table_file, 
		const std::string & magic1_table_file,
		const std::string & magic2_table_file,
		const std::string & magic3_table_file
		)
	{
		SparrowTingHuLogic & sthi = SparrowTingHuLogic::Instance();
		sthi.LoadHsiTables(
			table_file.c_str(), 
			magic1_table_file.c_str(),
			magic2_table_file.c_str(),
			magic3_table_file.c_str()
			);
	}

	static SparrowTingHuLogic & Instance()
	{
		static SparrowTingHuLogic s_sthl;
		return s_sthl;
	}

	static int ipow_of_10(int n)
	{
		if (n < 0 || n > 9) return 0;

		static int pow_v[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

		return pow_v[n];
	}

public:
	// hand_cards/magic: should be card data, not card index
	// �˺����ж�,*������*һ����֮��,���Դ����Щ���е�����һ������
	// hand_cardsΪ���Ƽ���,card_count�������Ȼ���� 3*n+2
	bool CalcTingableCardInfo(
		uint8_t * hand_cards, 
		int card_count, 
		uint8_t magic, 
		bool must_258, // ����258����
		bool use_feng, // ��������
		std::map<uint8_t, std::vector<uint8_t>> & result) OPTFUN
	{
		if (card_count == 0) return false;
		if (card_count % 3 != 2) return false;

		// ����ɫ����
		std::vector<uint8_t> classified_card_lists[4];

		int magic_count = 0;
		for (int i=0; i<card_count; ++i)
		{
			uint8_t card_data = hand_cards[i];
			if (card_data == magic) // ���������
			{
				magic_count++;
				continue;
			}
			
			uint8_t color = (card_data & 0xF0) >> 4;
			uint8_t value = card_data & 0x0F;
			classified_card_lists[color].push_back(card_data);
		}

		int non_magic_count = card_count - magic_count;

		result.clear();
		std::vector<uint8_t> hu_targets;

		// ÿ�黨ɫ����,����ȥ��һ��,����ʣ�µ����л�ɫ���������ܷ�����
		for (int i = 0; i < 4; i++)
		{
			std::vector<uint8_t> & classified_cards = classified_card_lists[i];

			int count = classified_cards.size();
			if (count == 0) continue;

			std::map<uint8_t, uint8_t> determined;
			for (int m = 0; m < count; m++)
			{
				uint8_t card = classified_cards[0];
				classified_cards.erase(classified_cards.begin());

				if (determined.find(card) == determined.end())
				{
					if (FindHuTargetsIfTingable(classified_card_lists, magic, magic_count, must_258, use_feng, hu_targets))
					{
						result[card] = hu_targets;
					}
					determined[card] = 1;
				}
				classified_cards.push_back(card);
			}
		}

		return result.size() > 0;
	}

private:
	int SignatureOfCardList(const std::vector<uint8_t> & cards, uint8_t added_card) OPTFUN
	{
		int signature = ipow_of_10((added_card & 0x0F) - 1); // ipow_of_10(-1) == 0
		for (std::vector<uint8_t>::const_iterator itr = cards.begin(); itr != cards.end(); itr++)
		{
			uint8_t card = *itr;
			signature += ipow_of_10((card & 0x0F) - 1);
		}
		return signature;
	}

	// ���������,����������������
	bool FindHuTargetsIfTingable(
		const std::vector<uint8_t> classified_cards_list[4], 
		uint8_t magic, 
		int magic_count, 
		bool must_258,
		bool use_feng,
		std::vector<uint8_t> & hu_targets) OPTFUN
	{
		int signature_mod[4] = { 0, 0, 0, FENG_SIGNATURE_CAP };
		hu_targets.clear();

		// ��ν�����ơ������Ǵ���һ���ƣ���������֮���ܺ�
		// ��ˣ�������п��ܵĵ����ƣ����ظ��ģ����������ƣ�����ܺ��ƣ�˵�����ơ��������Ÿղ��Լ������
		int loop_max = MJ_MAX_INDEX;
		if (!use_feng) loop_max -= MJ_FENG_COUNT; // 7 �ŷ���
		for (int i=0; i<loop_max; i++)
		{
			uint8_t color = i / 9;
			uint8_t value = i % 9 + 1;
			uint8_t card = (color << 4) | value;
			// if (card == magic) continue;

			// ��û�л��ӵ�����£���������ﲻ���������ƵĻ�ɫ�����Ȼ�����˻�ɫ���κ�һ����
			// �л��ӵ�ʱ���п��ܳ��ֺ������ơ�
			if (magic_count == 0 && classified_cards_list[color].size() == 0)
			{
				i = (color + 1) * 9 - 1;
				continue;
			}

			uint8_t added_cards[4] = { 0 };
			added_cards[color] = card;

			bool failed = false;
			std::vector<HU_SEGMENT_INFO> colored_found_segments[4];
			for (int j = 0; j < 4; j++) // j: enumerate 4 colors
			{
				if (classified_cards_list[j].size() + ((int)(!!added_cards[j])) == 0) continue;

				// �������ֻ�ӷ���ר�ñ����ֻ�п��ӵ�����
				int signature = SignatureOfCardList(classified_cards_list[j], added_cards[j]) + signature_mod[j];

				// ͬһ�����룬����ͬʱ�������޻��ӱ�ʹ����ӱ���
				// �������� [����][����] + �����ӣ�
				// �ж��ܷ���[����]��ʧ�ܣ�ʵ��������[����]��
				// �жϵĴ������ڣ�һ��[����]�����޻��ӱ����ҵ����ǽ��ԣ���[����]�ڴ����ӱ���Ҳ���ڽ��ԣ�
				// �����㷨�ж������� > 1 ���޷���[����]��
				// ʵ����һ��[����]��ȫ������Ϊ�����ӱ��е�[����]���ӣ���һ�Ż��ӣ����ټ��ϴ����ӱ��еĵ�[����]���ԣ��ɺ�[����]
				// ��˴�������Ҫ�������ƥ����Ͻ����ж�

				bool found = false;

				// �޻��Ӽ���������
				HuSegmentMap::iterator itr = m_hst.find(signature);
				if (itr != m_hst.end())
				{
					HU_SEGMENT_INFO & hsi = itr->second;
					if (magic_count == 0 && hsi.has_jiang && must_258 && !(hsi.jiang_258))
					{
						failed = true;
						break;
					}

					found = true;
					colored_found_segments[j].push_back(hsi);
				}

				// 1���ӱ�
				if (magic_count >= 1)
				{
					itr = m_hst_1magic.find(signature);
					if (itr != m_hst_1magic.end())
					{
						found = true;
						colored_found_segments[j].push_back(itr->second);
					}
				}

				// 2���ӱ�
				if (magic_count >= 2)
				{
					itr = m_hst_2magic.find(signature);
					if (itr != m_hst_2magic.end())
					{
						found = true;
						colored_found_segments[j].push_back(itr->second);
					}
				}

				// 3���ӱ�
				if (magic_count >= 3)
				{
					itr = m_hst_3magic.find(signature);
					if (itr != m_hst_3magic.end())
					{
						found = true;
						colored_found_segments[j].push_back(itr->second);
					}
				}

				if (!found)
				{
					failed = true;
					break;
				}
			}

			if (failed) continue;

			if (IsSegmentGroupHuable(colored_found_segments, must_258, magic_count))
			{
				hu_targets.push_back(card);
			}
		}

		return hu_targets.size() > 0;
	}

	bool IsSegmentGroupHuable(std::vector<HU_SEGMENT_INFO> colored_found_segments[4], bool must_258, int magic_count) OPTFUN
	{
		for (int k = 0; k < 4; ++k)
		{
			std::vector<HU_SEGMENT_INFO> & colored_hsi = colored_found_segments[k];
			if (colored_hsi.size() == 0)
			{
				// �ռ������һ���ٵ� HU_SEGMENT_INFO������֮��ѭ������
				HU_SEGMENT_INFO fake = { 0, 0, 0, 0 };
				colored_hsi.push_back(fake);
			}
		}

		int n_jiang = 0;
		int n_magic = 0;

		for (std::vector<HU_SEGMENT_INFO>::const_iterator itr0 = colored_found_segments[0].begin();
			itr0 != colored_found_segments[0].end(); itr0++)
		{
			const HU_SEGMENT_INFO & hsi0 = *itr0;
			n_jiang = hsi0.has_jiang;
			n_magic = hsi0.magic_count;
			if (must_258 && hsi0.has_jiang && hsi0.jiang_258) continue;

			for (std::vector<HU_SEGMENT_INFO>::const_iterator itr1 = colored_found_segments[1].begin();
				itr1 != colored_found_segments[1].end(); itr1++)
			{
				const HU_SEGMENT_INFO & hsi1 = *itr1;
				n_jiang += hsi1.has_jiang;
				if (n_jiang > 1) { n_jiang -= hsi1.has_jiang; continue; }
				if (must_258 && hsi1.has_jiang && hsi1.jiang_258) continue;
				n_magic += hsi1.magic_count;
				if (n_magic > magic_count) { n_magic -= hsi1.magic_count; continue; }

				for (std::vector<HU_SEGMENT_INFO>::const_iterator itr2 = colored_found_segments[2].begin();
					itr2 != colored_found_segments[2].end(); itr2++)
				{
					const HU_SEGMENT_INFO & hsi2 = *itr2;
					n_jiang += hsi2.has_jiang;
					if (n_jiang > 1) { n_jiang -= hsi2.has_jiang; continue; }
					if (hsi2.has_jiang && must_258 && hsi2.jiang_258) continue;
					n_magic += hsi2.magic_count;
					if (n_magic > magic_count) { n_magic -= hsi2.magic_count; continue; }

					for (std::vector<HU_SEGMENT_INFO>::const_iterator itr3 = colored_found_segments[3].begin();
						itr3 != colored_found_segments[3].end(); itr3++)
					{
						const HU_SEGMENT_INFO & hsi3 = *itr3;
						n_jiang += hsi3.has_jiang;
						if (n_jiang > 1) { n_jiang -= hsi3.has_jiang; continue; }
						if (hsi3.has_jiang && must_258 && hsi3.jiang_258) continue;
						n_magic += hsi3.magic_count;
						if (n_magic > magic_count) { n_magic -= hsi3.magic_count; continue; }

						// �ж��Ƿ��ܺ�: �������� 3*n + 2 ����ʽ
						int free_magic = magic_count - n_magic;
						if (n_jiang == 1 && free_magic % 3 == 0 // 1. �н��ԣ���ôʣ������Ʊ�����0����3��
							|| n_jiang == 0 && free_magic == 2) // 2. �޽��ԣ���ôʣ������Ʊ�����2�ţ��������Ϊ����
						{
							// ֻ��Ҫ�ҵ�һ������ܺ�����
							return true;
						}

						n_jiang -= hsi3.has_jiang;
						n_magic -= hsi3.magic_count;
					} // for itr3

					n_jiang -= hsi2.has_jiang;
					n_magic -= hsi2.magic_count;
				} // for itr2

				n_jiang -= hsi1.has_jiang;
				n_magic -= hsi1.magic_count;
			} // for itr1
		} // for itr0

		return false;
	}

	void LoadHsiTables(
		const char * table_name, 
		const char * magic1_table_name,
		const char * magic2_table_name,
		const char * magic3_table_name
		)
	{
		LoadHsiTable(m_hst, table_name);
		LoadHsiTable(m_hst_1magic, magic1_table_name);
		LoadHsiTable(m_hst_2magic, magic2_table_name);
		LoadHsiTable(m_hst_3magic, magic3_table_name);
	}

	void LoadHsiTable(HuSegmentMap & hst, const char * table_file) OPTFUN
	{
		hst.clear();

		FILE * f = fopen(table_file, "r+t");
		if (f == NULL) return;

		for (hst.clear();;)
		{
			int signature = 0;
			unsigned a, b, c, d = 0;
			HU_SEGMENT_INFO hsi = { 0 };
			int n = fscanf(
				f, 
				"%d, %u, %u, %u, %u\n",
				&signature, &a, &b, &c, &d
				);
			if (n != 5) break;

			hsi.has_shunzi = (uint8_t)a;
			hsi.has_jiang = (uint8_t)b;
			hsi.jiang_258 = (uint8_t)c;
			hsi.magic_count = (uint8_t)d;
			hst[signature] = hsi;
		}

		fclose(f);
		dout("%s: �������� %d ����¼��\n", table_file, hst.size());
	}

private:
	SparrowTingHuLogic() {}

private:
	HuSegmentMap m_hst;
	HuSegmentMap m_hst_1magic;
	HuSegmentMap m_hst_2magic;
	HuSegmentMap m_hst_3magic;
};

#endif/*HU_SEGMENT_H__*/
