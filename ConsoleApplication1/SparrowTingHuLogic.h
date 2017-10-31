#ifndef HU_SEGMENT_H__
#define HU_SEGMENT_H__

#pragma once

#ifdef __GNU_C__
#pragma GCC push_options
#pragma GCC optimize ("O2")
#endif

#include <stdint.h>
#include <map>
#include <vector>
#include <string>

#ifndef dout
#define dout CCLOG
#endif

// 0 - 饼, 1 - 万, 2 - 条, 3 - 风
//#define COLOR_BING	0
//#define COLOR_WAN	1
//#define COLOR_TIAO	2
//#define COLOR_FENG	3

#define MJ_MAX_INDEX	34
#define MJ_FENG_COUNT	7
#define MJ_MAX_HANDCARDS 14

//#ifdef __GNU_C__
//#define OPTFUN __attribute__((optimize(2)))
//#else
#define OPTFUN
//#endif

// 特征值 signature:
// 牌所在花色的序号为1~9(风牌为1~7),例如三万的序号是3,发财的序号是6,等等
// 那么取一个最多9位的十进制数描述集合中的所有牌,用其位数表达牌的序号,位上的数值表达所在序号牌的数量
// 例如,如果集合(注意本集合内的牌都是同一花色的)中的牌是“二万、二万、五万、六万、七万”，
// 则取十进制数001110020表达该集合，其中：
// 个位表达序号为1的牌数量，此处集合中没有一万这张牌，所以个位为0，
// 十位表达序号为2的牌数量，此处集合中，有两个二万，所以十位为2，以此类推。
// 注：十亿位为1时表达风牌专用表项，因为风牌只有刻子，没有顺子

// [[ 1、2、3张混子情况分开保存，以下方案不再使用 <<--
// **然而**
// 对于某花色片段剩单张的情况，则既有可能是将对扣一张混子，也可能是刻子或顺子扣两张混子，特征码一致
// 因此，将特征值增加1个十进制位，成为10位数，第10位，即十亿位表达扣除的混子个数，即：
// 对于无混子特征值，十亿为为0，一混子为1，以此类推，最多3混子。
// -->> ]]

struct HU_SEGMENT_INFO {
    uint8_t has_shunzi;	// 含顺子
    uint8_t has_jiang;	// 含将对
    uint8_t jiang_258;	// 将对是258
    uint8_t magic_count;// 需要几个混子牌
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
    
    static void InitWithData(const char * data,
                             const char * data_1magic,
                             const char * data_2magic,
                             const char * data_3magic
                             )
    {
        if (!data || !data_1magic || !data_2magic || !data_3magic)
        {
            dout("[SPARROW] InitWithData: 请提供有效的数据。\n");
            return;
        }
        
        SparrowTingHuLogic & sthi = SparrowTingHuLogic::Instance();
        sthi.LoadHsiTablesFromData(
                           data,
                           data_1magic,
                           data_2magic,
                           data_3magic
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
    void MergeTingInfo(std::map<uint8_t, std::vector<uint8_t>> & tf1, const std::map<uint8_t, std::vector<uint8_t>> & tf2)
    {
        if (tf2.size() == 0) return;
        
        for (std::map<uint8_t, std::vector<uint8_t>>::const_iterator itr = tf2.begin(); itr != tf2.end(); itr++)
        {
            if (tf1.count(itr->first) == 0)
            {
                tf1[itr->first] = itr->second;
            }
            else
            {
                MergeTingList(tf1[itr->first], itr->second);
            }
        }
    }
    
    void MergeTingList(std::vector<uint8_t> & tl1, const std::vector<uint8_t> & tl2)
    {
        if (tl2.size() == 0) return;
        
        std::map<uint8_t, bool> pool;
        for (uint8_t u : tl1)
        {
            pool[u] = true;
        }
        
        for (uint8_t u : tl2)
        {
            if (pool.count(u) == 0)
            {
                tl1.push_back(u);
            }
        }
    }
    
    // 判断手牌打出分别打出哪张牌能听七对
    // 参数含义与 CalcTingableCardInfo 相同
    bool OPTFUN CalcSevenPairTingInfo(
                                      uint8_t * hand_cards,
                                      int card_count,
                                      uint8_t magic,
                                      bool use_feng, // 包括风牌
                                      std::map<uint8_t, std::vector<uint8_t>> & result)
    {
        if (card_count != MJ_MAX_HANDCARDS) return false; // 七对不能下坎
        
        int magic_count = 0;
        std::map<uint8_t, int8_t> counted_cards;
        for (int i=0; i<card_count; ++i)
        {
            uint8_t card = hand_cards[i];
            if (card == magic)
            {
                magic_count++;
                continue;
            }
            counted_cards[card]++;
        }
        
        std::map<uint8_t, int8_t> singled_cards;
        std::map<uint8_t, int8_t> doubled_cards;
        for (std::map<uint8_t, int8_t>::const_iterator itr = counted_cards.begin(); itr != counted_cards.end(); itr++)
        {
            if (itr->second % 2 == 0)
                doubled_cards[itr->first] = itr->second;
            else
                singled_cards[itr->first] = itr->second;
        }
        
        // 总数14张，diff必然为偶数
        int diff = ((int)singled_cards.size()) - magic_count;
        if (diff > 2) return false;
        
        if (diff == 2)
        {
            // 打出任意一张落单牌可以胡所有其他落单牌
            for (std::map<uint8_t, int8_t>::const_iterator itr = singled_cards.begin(); itr != singled_cards.end(); itr++)
            {
                std::vector<uint8_t> & ting_list = result[itr->first];
                for (std::map<uint8_t, int8_t>::const_iterator itr2 = singled_cards.begin(); itr2 != singled_cards.end(); itr2++)
                {
                    if (itr2->first != itr->first) ting_list.push_back(itr2->first);
                }
            }

			return true;
        }
        
		// diff == 0 or diff < 0
        // 其实可以胡牌，不过如果用户不想胡，那还是要计算听牌信息

		// 无癞子情况
		if (magic == 0 || magic_count == 0)
		{
			// 可打出任意一张未落单牌并听该张牌
			PlayOutAnyCardAndTingItself(doubled_cards, result);
			return true;
		}

		uint8_t full_cards[] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x31,0x32,0x33,0x34,0x35,0x36,0x37};
		int max_cards = sizeof(full_cards)/sizeof(full_cards[0]);
		if (!use_feng) max_cards = max_cards - MJ_FENG_COUNT;
        if (diff < 0)
        {
            // 打出任意一张手牌可以胡任意牌
			counted_cards[magic] = magic_count;
			for (std::map<uint8_t, int8_t>::const_iterator itr = counted_cards.begin(); itr != counted_cards.end(); itr++)
			{
				std::vector<uint8_t> & r = result[itr->first];
				for (int n = 0; n < max_cards; ++n)
				{
					r.push_back(full_cards[n]);
				}
			}
			counted_cards.erase(magic);
        }
        else
        {
			// 打出任何一张落单牌则可以胡任意牌；
			for (std::map<uint8_t, int8_t>::const_iterator itr = singled_cards.begin(); itr != singled_cards.end(); itr++)
			{
				int max_cards = use_feng ? 34 : 27;
				std::vector<uint8_t> & r = result[itr->first];
				for (int n = 0; n < max_cards; ++n)
				{
					r.push_back(full_cards[n]);
				}
			}

			// 打出任意一张非单牌可以胡本身及任意一张落单牌
			for (std::map<uint8_t, int8_t>::const_iterator itr = doubled_cards.begin(); itr != doubled_cards.end(); itr++)
			{
				std::vector<uint8_t> & r = result[itr->first];
				r.push_back(itr->first);
				for (std::map<uint8_t, int8_t>::const_iterator itr2 = singled_cards.begin(); itr2 != singled_cards.end(); itr2++)
				{
					r.push_back(itr2->first);
				}
			}

            // 打出一张赖子牌可以胡任意落单牌
            std::vector<uint8_t> & r = result[magic];
            for (std::map<uint8_t, int8_t>::const_iterator itr = singled_cards.begin(); itr != singled_cards.end(); itr++)
            {
                r.push_back(itr->first);
            }
        }
        
        return true;
    }
    
    // hand_cards/magic: should be card data, not card index
    // 此函数判断,*摸上来*一张牌之后,可以打出哪些牌中的任意一张听牌
    // hand_cards为手牌集合,card_count张数则必然符合 3*n+2
    bool OPTFUN CalcTingableCardInfo(
                              uint8_t * hand_cards,
                              int card_count,
                              uint8_t magic,
                              bool must_258, // 必须258做将
                              bool use_feng, // 包括风牌
                              std::map<uint8_t, std::vector<uint8_t>> & result)
    {
        if (card_count == 0) return false;
        if (card_count % 3 != 2) return false;
        
        // 按花色分组
        std::vector<uint8_t> classified_card_lists[4];
        
        int magic_count = 0;
        for (int i=0; i<card_count; ++i)
        {
            uint8_t card_data = hand_cards[i];
            if (card_data == magic) // 过滤癞子牌
            {
                magic_count++;
                continue;
            }
            uint8_t color = (card_data & 0xF0) >> 4;
            classified_card_lists[color].push_back(card_data);
        }
        
        result.clear();
        std::vector<uint8_t> hu_targets;
        
        // 每组花色的牌,依次去掉一张,看看剩下的所有花色的所有牌能否听牌
        for (int i = 0; i < 4; i++)
        {
            std::vector<uint8_t> & classified_cards = classified_card_lists[i];
            
            int count = (int)classified_cards.size();
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
    void PlayOutAnyCardAndTingItself(const std::map<uint8_t, int8_t> classified_cards, std::map<uint8_t, std::vector<uint8_t>> & ting_result)
    {
        for (std::map<uint8_t, int8_t>::const_iterator itr = classified_cards.begin(); itr != classified_cards.end(); itr++)
        {
            ting_result[itr->first] = std::vector<uint8_t>(1, itr->first);
        }
    }
    
    int OPTFUN SignatureOfCardList(const std::vector<uint8_t> & cards, uint8_t added_card)
    {
        int signature = ipow_of_10((added_card & 0x0F) - 1); // ipow_of_10(-1) == 0
        for (std::vector<uint8_t>::const_iterator itr = cards.begin(); itr != cards.end(); itr++)
        {
            uint8_t card = *itr;
            signature += ipow_of_10((card & 0x0F) - 1);
        }
        return signature;
    }
    
    // 如果能听牌,查找听胡的所有牌
    bool OPTFUN FindHuTargetsIfTingable(
                                 const std::vector<uint8_t> classified_cards_list[4],
                                 uint8_t magic,
                                 int magic_count,
                                 bool must_258,
                                 bool use_feng,
                                 std::vector<uint8_t> & hu_targets)
    {
        int signature_mod[4] = { 0, 0, 0, FENG_SIGNATURE_CAP };
        hu_targets.clear();
        
        // 所谓“听牌”，就是存在一张牌，加入手牌之后能胡
        // 因此，穷举所有可能的单张牌（不重复的），加入手牌，如果能胡牌，说明手牌“听”这张刚测试加入的牌
        int loop_max = MJ_MAX_INDEX;
        if (!use_feng) loop_max -= MJ_FENG_COUNT; // 7 张风牌
        for (int i=0; i<loop_max; i++)
        {
            uint8_t color = i / 9;
            uint8_t value = i % 9 + 1;
            uint8_t card = (color << 4) | value;
            // if (card == magic) continue;
            
            // 在没有混子的情况下，如果手牌里不包含测试牌的花色，则必然不听此花色的任何一张牌
            // 有混子的时候，有可能出现胡任意牌。
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
                
                // 风牌组合只从风牌专用表项（即只有刻子的项）里查
                int signature = SignatureOfCardList(classified_cards_list[j], added_cards[j]) + signature_mod[j];
                
                // 同一特征码，可能同时存在于无混子表和带混子表中
                // 例如手牌 [三万][五条] + 两混子，
                // 判断能否听[三万]会失败，实际是能听[三万]的
                // 判断的错误在于，一对[三万]先在无混子表中找到，是将对；单[五条]在带混子表中也存在将对，
                // 这样算法判定将对数 > 1 ，无法胡[三万]。
                // 实际上一对[三万]完全可以作为带混子表中的[三万]刻子（扣一张混子），再加上带混子表中的单[五条]将对，可胡[三万]
                // 因此此问题需要穷举所有匹配组合进行判断
                
                bool found = false;
                
                // 无混子集合里的情况
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
                
                // 1混子表
                if (magic_count >= 1)
                {
                    itr = m_hst_1magic.find(signature);
                    if (itr != m_hst_1magic.end())
                    {
                        found = true;
                        colored_found_segments[j].push_back(itr->second);
                    }
                }
                
                // 2混子表
                if (magic_count >= 2)
                {
                    itr = m_hst_2magic.find(signature);
                    if (itr != m_hst_2magic.end())
                    {
                        found = true;
                        colored_found_segments[j].push_back(itr->second);
                    }
                }
                
                // 3混子表
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
    
    bool OPTFUN IsSegmentGroupHuable(std::vector<HU_SEGMENT_INFO> colored_found_segments[4], bool must_258, int magic_count)
    {
        for (int k = 0; k < 4; ++k)
        {
            std::vector<HU_SEGMENT_INFO> & colored_hsi = colored_found_segments[k];
            if (colored_hsi.size() == 0)
            {
                // 空集合添加一个假的 HU_SEGMENT_INFO，便于之后循环计算
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
                        
                        // 判断是否能胡: 必须满足 3*n + 2 的形式
                        int free_magic = magic_count - n_magic;
                        if ((n_jiang == 1 && free_magic % 3 == 0) // 1. 有将对，那么剩余癞子牌必须是0或者3张
                            || (n_jiang == 0 && free_magic == 2)) // 2. 无将对，那么剩余癞子牌必须是2张：癞子牌作为将对
                        {
                            // 只需要找到一个组合能胡即可
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
    
    void LoadHsiTables(const std::string & table_name,
                       const std::string & magic1_table_name,
                       const std::string & magic2_table_name,
                       const std::string & magic3_table_name
                       )
    {
        LoadHsiTable(m_hst, table_name);
        LoadHsiTable(m_hst_1magic, magic1_table_name);
        LoadHsiTable(m_hst_2magic, magic2_table_name);
        LoadHsiTable(m_hst_3magic, magic3_table_name);
    }
    
    void LoadHsiTablesFromData(const char * data,
                               const char * data_1maigc,
                               const char * data_2maigc,
                               const char * data_3maigc
                               )
    {
        LoadHsiTableFromData(m_hst, data);
        LoadHsiTableFromData(m_hst_1magic, data_1maigc);
        LoadHsiTableFromData(m_hst_2magic, data_2maigc);
        LoadHsiTableFromData(m_hst_3magic, data_3maigc);
    }
    
    void OPTFUN LoadHsiTable(HuSegmentMap & hst, const std::string & table_file)
    {
        // hst.clear();
        if (hst.size() > 0) return;
        
        FILE * f = fopen(table_file.c_str(), "r+t");
        if (f == NULL)
        {
            dout("[SPARROW] cannot open file: %s\n", table_file.c_str());
            return;
        }
        
        for (;;)
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
        dout("[SPARROW] %s: 共加载了 %ld 条记录。\n", table_file.c_str(), (long)hst.size());
    }
    
    void OPTFUN LoadHsiTableFromData(HuSegmentMap & hst, const char * data)
    {
        if (hst.size() > 0)
        {
            dout("[SPARROW] LoadHsiTableFromData: table already loaded. call clear() before reload.\n");
            return;
        }
        
        if (data == NULL)
        {
            dout("[SPARROW] empty map data\n");
            return;
        }
        
        // NDK-r9d has corrupted sscanf() implementation,
        // while this project is not compatible with newer version of NDK,
        // so use an alternative way here.
        // int last_signature = 0;
        char targets[] = { ',', ',', ',', ',', '\n' };
        for (const char * sp = data; *sp;)
        {
            int n[5] = { 0 };
            int parsed = 0;
            for (int i = 0; i< 5; i++)
            {
                char * p = NULL;
                n[i] = (int)strtol(sp, &p, 10);
                if (p > sp) parsed++;
                
                sp = strchr(p, targets[i]);
                if (!sp) break;
                sp++;
            }
            
            if (parsed != 5) break;
            
            int signature = n[0];
            HU_SEGMENT_INFO hsi;
            hsi.has_shunzi = n[1];
            hsi.has_jiang = (uint8_t)n[2];
            hsi.jiang_258 = (uint8_t)n[3];
            hsi.magic_count = (uint8_t)n[4];
            hst[signature] = hsi;
            
            // last_signature = signature;
        }
//        HU_SEGMENT_INFO hsi = hst[last_signature];
//        dout("[SPARROW] last line: %d, %d, %d, %d, %d\n", last_signature, (int)hsi.has_shunzi, (int)hsi.has_jiang, (int)hsi.jiang_258, (int)hsi.magic_count);
        dout("[SPARROW] 共加载了 %ld 条记录。\n", (long)hst.size());
    }
    
private:
    SparrowTingHuLogic() {}
    
private:
    HuSegmentMap m_hst;
    HuSegmentMap m_hst_1magic;
    HuSegmentMap m_hst_2magic;
    HuSegmentMap m_hst_3magic;
};

#ifdef __GNU_C__
#pragma GCC reset_options
#endif

#endif/*HU_SEGMENT_H__*/
