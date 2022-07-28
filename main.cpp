#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <algorithm>
#include "jsoncpp/json.h"

using std::set;
using std::sort;
using std::string;
using std::unique;
using std::vector;

enum class Stage {
    BIDDING, // 叫分阶段
    PLAYING     // 打牌阶段
};

enum class CardComboType {
    PASS,        // 过
    SINGLE,        // 单张
    PAIR,        // 对子
    STRAIGHT,    // 顺子
    STRAIGHT2,    // 双顺
    TRIPLET,    // 三条
    TRIPLET1,    // 三带一
    TRIPLET2,    // 三带二
    BOMB,        // 炸弹
    QUADRUPLE2, // 四带二（只）
    QUADRUPLE4, // 四带二（对）
    PLANE,        // 飞机
    PLANE1,        // 飞机带小翼
    PLANE2,        // 飞机带大翼
    SSHUTTLE,    // 航天飞机
    SSHUTTLE2,    // 航天飞机带小翼
    SSHUTTLE4,    // 航天飞机带大翼
    ROCKET,        // 火箭
    INVALID        // 非法牌型
};

int cardComboScores[] = {
    0,    // 过
    1,    // 单张
    2,    // 对子
    6,    // 顺子
    6,    // 双顺
    4,    // 三条
    4,    // 三带一
    4,    // 三带二
    10, // 炸弹
    8,    // 四带二（只）
    8,    // 四带二（对）
    8,    // 飞机
    8,    // 飞机带小翼
    8,    // 飞机带大翼
    10, // 航天飞机（需要特判：二连为10分，多连为20分）
    10, // 航天飞机带小翼
    10, // 航天飞机带大翼
    16, // 火箭
    0    // 非法牌型
};

using Card = short;
const Card card_joker = 52;
const Card card_JOKER = 53;

using Level = short;
const Level MAX_LEVEL = 15;
const Level MAX_STRAIGHT_LEVEL = 11;
const Level level_joker = 13;
const Level level_JOKER = 14;

Level c2l(Card card) {return card / 4 + card / 53;}

int oppocnt[MAX_LEVEL];

struct CardCombo {
    struct CardPack {
        Level level;
        short count;

        bool operator <(const CardPack &b) const {
            if (count == b.count)
                return level > b.level;
            return count > b.count;
        }
    };
    vector<Card> cards;         // 原始的牌，未排序
    vector<CardPack> packs;     // 按数目和大小排序的牌种
    CardComboType comboType; // 算出的牌型
    Level comboLevel = 0;     // 算出的大小序

    int findMaxSeq() const
    {
        for (unsigned c = 1; c < packs.size(); c++)
            if (packs[c].count != packs[0].count ||
                packs[c].level != packs[c - 1].level - 1)
                return c;
        return packs.size();
    }

    CardCombo() : comboType(CardComboType::PASS) {}

    template <typename CARD_ITERATOR>
    CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
    {
        // 特判：空
        if (begin == end)
        {
            comboType = CardComboType::PASS;
            return;
        }

        // 每种牌有多少个
        short counts[MAX_LEVEL + 1] = {};

        // 同种牌的张数（有多少个单张、对子、三条、四条）
        short countOfCount[5] = {};

        cards = vector<Card>(begin, end);
        for (Card c : cards)
            counts[c2l(c)]++;
        for (Level l = 0; l <= MAX_LEVEL; l++)
            if (counts[l])
            {
                packs.push_back(CardPack{l, counts[l]});
                countOfCount[counts[l]]++;
            }
        sort(packs.begin(), packs.end());

        // 用最多的那种牌总是可以比较大小的
        comboLevel = packs[0].level;

        // 计算牌型
        // 按照 同种牌的张数 有几种 进行分类
        vector<int> kindOfCountOfCount;
        for (int i = 0; i <= 4; i++)
            if (countOfCount[i])
                kindOfCountOfCount.push_back(i);
        sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

        int curr, lesser;

        switch (kindOfCountOfCount.size())
        {
        case 1: // 只有一类牌
            curr = countOfCount[kindOfCountOfCount[0]];
            switch (kindOfCountOfCount[0])
            {
            case 1:
                // 只有若干单张
                if (curr == 1)
                {
                    comboType = CardComboType::SINGLE;
                    return;
                }
                if (curr == 2 && packs[1].level == level_joker)
                {
                    comboType = CardComboType::ROCKET;
                    return;
                }
                if (curr >= 5 && findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::STRAIGHT;
                    return;
                }
                break;
            case 2:
                // 只有若干对子
                if (curr == 1)
                {
                    comboType = CardComboType::PAIR;
                    return;
                }
                if (curr >= 3 && findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::STRAIGHT2;
                    return;
                }
                break;
            case 3:
                // 只有若干三条
                if (curr == 1)
                {
                    comboType = CardComboType::TRIPLET;
                    return;
                }
                if (findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::PLANE;
                    return;
                }
                break;
            case 4:
                // 只有若干四条
                if (curr == 1)
                {
                    comboType = CardComboType::BOMB;
                    return;
                }
                if (findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::SSHUTTLE;
                    return;
                }
            }
            break;
        case 2: // 有两类牌
            curr = countOfCount[kindOfCountOfCount[1]];
            lesser = countOfCount[kindOfCountOfCount[0]];
            if (kindOfCountOfCount[1] == 3)
            {
                // 三条带？
                if (kindOfCountOfCount[0] == 1)
                {
                    // 三带一
                    if (curr == 1 && lesser == 1)
                    {
                        comboType = CardComboType::TRIPLET1;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::PLANE1;
                        return;
                    }
                }
                if (kindOfCountOfCount[0] == 2)
                {
                    // 三带二
                    if (curr == 1 && lesser == 1)
                    {
                        comboType = CardComboType::TRIPLET2;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::PLANE2;
                        return;
                    }
                }
            }
            if (kindOfCountOfCount[1] == 4)
            {
                // 四条带？
                if (kindOfCountOfCount[0] == 1)
                {
                    // 四条带两只 * n
                    if (curr == 1 && lesser == 2)
                    {
                        comboType = CardComboType::QUADRUPLE2;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr * 2 &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::SSHUTTLE2;
                        return;
                    }
                }
                if (kindOfCountOfCount[0] == 2)
                {
                    // 四条带两对 * n
                    if (curr == 1 && lesser == 2)
                    {
                        comboType = CardComboType::QUADRUPLE4;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr * 2 &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::SSHUTTLE4;
                        return;
                    }
                }
            }
        }

        comboType = CardComboType::INVALID;
    }

    bool operator <(const CardCombo &b) const
    {
        if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
            return false;
        if (b.comboType == CardComboType::ROCKET)
            return true;
        if (b.comboType == CardComboType::BOMB)
            switch (comboType)
            {
            case CardComboType::ROCKET:
                return false;
            case CardComboType::BOMB:
                return b.comboLevel > comboLevel;
            default:
                return true;
            }
        return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
    }

    bool unbe() {
        int l = findMaxSeq(), c = packs[0].count;
        int tmp = MAX_LEVEL;
        if (comboType == CardComboType::STRAIGHT || comboType == CardComboType::STRAIGHT2 || comboType == CardComboType::PLANE || comboType == CardComboType::PLANE1 || comboType == CardComboType::PLANE2) {
            tmp = MAX_STRAIGHT_LEVEL + 1;
            if (packs[0].level == MAX_STRAIGHT_LEVEL) return true;
        }
        for (int i = packs[l - 1].level + 1; i < tmp; ++i) {
            bool beaten = true;
            for (int j = 0; j < l; ++j)
                if (i + j >= tmp || (i + j < tmp && oppocnt[i + j] < c)) {beaten = false; break;}
            if (beaten) return false;
        }
        return true;
    }
};

struct ComboSet {
    vector<Card> cards;
    double value, cntC; int cntSmall;
    short count[MAX_LEVEL], countOfCount[5];

    template <typename Card_IT>
    ComboSet(Card_IT begin, Card_IT end) {
        cards = vector<Card>(begin, end);
        sort(cards.begin(), cards.end());
        value = -120.0, cntC = 30.0, cntSmall = 0;
        memset(count, 0, sizeof(count));
        memset(countOfCount, 0, sizeof(countOfCount));
        vector<Card> two, joker, packs[5];

        for (int i = 0; i < cards.size(); ++i) {
            Card c = cards[i]; count[c2l(c)]++;
            if (c2l(c) == 12) two.push_back(c);
            if (c2l(c) > 12) joker.push_back(c);
        }
        for (int i = 0; i < MAX_LEVEL; ++i) countOfCount[count[i]]++;
        for (Card c : cards) {
            if (c2l(c) <= MAX_STRAIGHT_LEVEL) packs[count[c2l(c)]].push_back(c);
        }
        int num[5] = {0};
        for (int i = 0; i < 5; ++i) sort(packs[i].begin(), packs[i].end()), num[i] = packs[i].size();

        int pow3[] = {1, 3, 9, 27, 81, 243, 729}, pow5[] = {1, 5, 25, 125, 625, 3125, 15625};
        for (int q2 = 0; q2 < (1 << (num[2] / 2)); ++q2)
            for (int q3 = 0; q3 < pow3[num[3] / 3]; ++q3)
                for (int q4 = 0; q4 < pow5[num[4] / 4]; ++q4) {
                    vector<Card> sg = packs[1], pr, tri, qu;
                    int tmp = q2;
                    for (int r = 0; r < num[2]; r += 2) {
                        if (tmp % 2 == 0) {
                            sg.push_back(packs[2][r]), sg.push_back(packs[2][r + 1]);
                        } else {
                            pr.push_back(packs[2][r]), pr.push_back(packs[2][r + 1]);
                        }
                    }
                    tmp = q3;
                    for (int r = 0; r < num[3]; r += 3) {
                        if (tmp % 3 == 0) {
                            sg.push_back(packs[3][r]), sg.push_back(packs[3][r + 1]), sg.push_back(packs[3][r + 2]);
                        } else if (tmp % 3 == 1) {
                            sg.push_back(packs[3][r]), pr.push_back(packs[3][r + 1]), pr.push_back(packs[3][r + 2]);
                        } else if (tmp % 3 == 2) {
                            tri.push_back(packs[3][r]), tri.push_back(packs[3][r + 1]), tri.push_back(packs[3][r + 2]);
                        }
                        tmp /= 3;
                    }
                    tmp = q4;
                    for (int r = 0; r < num[4]; r += 4) {
                        if (tmp % 5 == 0) {
                            sg.push_back(packs[4][r]), sg.push_back(packs[4][r + 1]), sg.push_back(packs[4][r + 2]), sg.push_back(packs[4][r + 3]);
                        } else if (tmp % 5 == 1) {
                            sg.push_back(packs[4][r]), sg.push_back(packs[4][r + 1]), pr.push_back(packs[4][r + 2]), pr.push_back(packs[4][r + 3]);
                        } else if (tmp % 5 == 2) {
                            pr.push_back(packs[4][r]), pr.push_back(packs[4][r + 1]), pr.push_back(packs[4][r + 2]), pr.push_back(packs[4][r + 3]);
                        } else if (tmp % 5 == 3) {
                            sg.push_back(packs[4][r]), tri.push_back(packs[4][r + 1]), tri.push_back(packs[4][r + 2]), tri.push_back(packs[4][r + 3]);
                        } else if (tmp % 5 == 4) {
                            qu.push_back(packs[4][r]), qu.push_back(packs[4][r + 1]), qu.push_back(packs[4][r + 2]), qu.push_back(packs[4][r + 3]);
                        }
                        tmp /= 5;
                    }
                    sort(sg.begin(), sg.end());
                    vector<Card> tstr1[5]; int tcntstr1 = 0;
                    for (int i = 0; i < sg.size(); ++i) {
                        if (sg[i] == 80) continue;
                        vector<Card> tmp; int j = i + 1; tmp.push_back(sg[i]);
                        vector<int> erasor;
                        while (j < sg.size() && tmp.size() < 5) {
                            if (sg[j] == 80) {++j; continue;}
                            if (c2l(sg[j]) == c2l(tmp.back())) ++j;
                            else if (c2l(sg[j]) == c2l(tmp.back()) + 1) tmp.push_back(sg[j]), erasor.push_back(j), ++j;
                            else break;
                        }
                        if (tmp.size() == 5) {
                            tstr1[tcntstr1++] = tmp;
                            for (int k = 0; k < erasor.size(); ++k) sg[erasor[k]] = 80;
                        }
                    }

                    for (int i = 0; i < sg.size(); ++i) {
                        if (sg[i] == 80) continue;
                        for (int j = 0; j < tcntstr1; ++j) {
                            if (c2l(sg[i]) == c2l(tstr1[j].back()) + 1) {tstr1[j].push_back(sg[i]), sg[i] = 80; break;}
                        }
                    }

                    for (int i = 0; i < tcntstr1; ++i)
                        for (int j = 0; j < tcntstr1; ++j) {
                            if (i == j || tstr1[i].empty() || tstr1[j].empty()) continue;
                            if (c2l(tstr1[i].back()) == c2l(tstr1[j].front()) - 1) {
                                for (int k = 0; k < tstr1[j].size(); ++k) tstr1[i].push_back(tstr1[j][k]);
                                tstr1[j].clear();
                            }
                        }

                    vector<Card> str1[5]; int cntstr1 = 0;
                    for (int i = 0; i < tcntstr1; ++i)
                        if (!tstr1[i].empty()) str1[cntstr1++] = tstr1[i];

                    sort(sg.begin(), sg.end());
                    while (!sg.empty() && sg.back() == 80) sg.pop_back();

                    sort(pr.begin(), pr.end());
                    vector<Card> tstr2[5]; int tcntstr2 = 0;
                    for (int i = 0; i < pr.size(); i += 2) {
                        if (pr[i] == 80) continue;
                        vector<Card> tmp; int j = i + 2; tmp.push_back(pr[i]), tmp.push_back(pr[i + 1]);
                        vector<int> erasor;
                        while (j < pr.size() && tmp.size() < 6) {
                            if (pr[j] == 80) {j += 2; continue;}
                            if (c2l(pr[j]) == c2l(tmp.back())) j += 2;
                            else if (c2l(pr[j]) == c2l(tmp.back()) + 1) {
                                tmp.push_back(pr[j]), tmp.push_back(pr[j + 1]);
                                erasor.push_back(j), erasor.push_back(j + 1), j += 2;
                            }
                            else break;
                        }
                        if (tmp.size() == 6) {
                            tstr2[tcntstr2++] = tmp;
                            for (int k = 0; k < erasor.size(); ++k) pr[erasor[k]] = 80;
                        }
                    }

                    for (int i = 0; i < pr.size(); i += 2) {
                        if (pr[i] == 80) continue;
                        for (int j = 0; j < tcntstr2; ++j) {
                            if (c2l(pr[i]) == c2l(tstr2[j].back()) + 1) {
                                tstr2[j].push_back(pr[i]), tstr2[j].push_back(pr[i + 1]);
                                pr[i] = 80, pr[i + 1] = 80; break;
                            }
                        }
                    }

                    for (int i = 0; i < tcntstr2; ++i)
                        for (int j = 0; j < tcntstr2; ++j) {
                            if (i == j || tstr2[i].empty() || tstr2[j].empty()) continue;
                            if (c2l(tstr2[i].back()) == c2l(tstr2[j].front()) - 1) {
                                for (int k = 0; k < tstr2[j].size(); ++k) tstr2[i].push_back(tstr2[j][k]);
                                tstr2[j].clear();
                            }
                        }

                    vector<Card> str2[5]; int cntstr2 = 0;
                    for (int i = 0; i < tcntstr2; ++i)
                        if (!tstr2[i].empty()) str2[cntstr2++] = tstr2[i];

                    sort(pr.begin(), pr.end());
                    while (!pr.empty() && pr.back() == 80) pr.pop_back();

                    double tmpval = 0, tmpC = 0; int tmpCint = 0, tmpSmall = 0;
                    vector<Card> tmpvec;

                    for (int i = 0; i < sg.size(); ++i) {
                        tmpvec.push_back(c2l(sg[i]));
                        Card c[] = {sg[i]};
                        if (!CardCombo(c, c + 1).unbe()) ++tmpSmall;
                    }
                    for (int i = 0; i < pr.size(); i += 2) {
                        tmpvec.push_back(c2l(pr[i]));
                        Card c[] = {pr[i], pr[i + 1]};
                        if (!CardCombo(c, c + 2).unbe()) ++tmpSmall;
                    }
                    sort(tmpvec.begin(), tmpvec.end());

                    if (two.size() == 3) {tri.push_back(two[0]), tri.push_back(two[1]), tri.push_back(two[2]);}
                    tmpCint = pr.size() / 2 + sg.size() - tri.size() / 3; tmpCint = (tmpCint < 0 ? 0 : tmpCint);
                    tmpSmall -= tri.size() / 3; if (tmpSmall < 0) tmpSmall = 0;
                    for (int i = 0; i < tmpCint; ++i) {
                        tmpval += (double)(tmpvec.back() - 7);
                        tmpvec.pop_back();
                    }

                    tmpC = (double)tmpCint;
                    if (two.size() == 4) {qu.push_back(two[0]), qu.push_back(two[1]), qu.push_back(two[2]), qu.push_back(two[3]);}
                    tmpC += cntstr1 + cntstr2 + tri.size() / 3 + qu.size() / 4;

                    for (int i = 0; i < tri.size(); i += 3) {
                        tmpval += (double)(c2l(tri[i]) - 7);
                        if (i >= 3 && c2l(tri[i]) - c2l(tri[i - 3]) == 1) tmpC -= 0.1;
                    }
                    for (int i = 0; i < qu.size(); i += 4) tmpval += (double)(c2l(qu[i]) + 7);

                    for (int i = 0; i < cntstr1; ++i) tmpval += (double)(c2l(str1[i].back()) - 6);
                    for (int i = 0; i < cntstr2; ++i) {
                        tmpval += (double)(c2l(str2[i].back()) - 6);
                        tmpC += (double)str2[i].size() * 0.1;
                    }

                    if (two.size() == 1) {
                        sg.push_back(two[0]);
                        tmpC += 1.0, tmpval += 5.0;
                    }
                    if (two.size() == 2) {
                        pr.push_back(two[0]), pr.push_back(two[1]);
                        tmpC += 1.0, tmpval += 5.0;
                    }
                    if (!joker.empty()) {
                        tmpC += 1.0;
                        if (joker.size() == 2) tmpval += 20.0;
                        else tmpval += (joker[0] == 52 ? 6.0 : 7.0);
                    }
                    if ((value - 5.0 * cntC) < (tmpval - 5.0 * tmpC)) {
 //                       std::cout << tmpval << ' ' << tmpC << '\n';
                        for (int i = 0; i < sg.size(); ++i) {
                            Card c[] = {sg[i]};
                        }
                        for (int i = 0; i < pr.size(); i += 2) {
                            Card c[] = {pr[i], pr[i + 1]};
                        }
                        for (int i = 0; i < tri.size(); i += 3) {
                            Card c[] = {tri[i], tri[i + 1], tri[i + 2]};
                            if (!CardCombo(c, c + 3).unbe()) ++tmpSmall;
                        }
                        for (int i = 0; i < qu.size(); i += 4) {
                            Card c[] = {qu[i], qu[i + 1], qu[i + 2], qu[i + 3]};
                            if (!CardCombo(c, c + 4).unbe()) ++tmpSmall;
                        }
                        for (int i = 0; i < cntstr1; ++i)
                            if (!CardCombo(str1[i].begin(), str1[i].end()).unbe()) ++tmpSmall;
                        for (int i = 0; i < cntstr2; ++i)
                            if (!CardCombo(str2[i].begin(), str2[i].end()).unbe()) ++tmpSmall;

                        if (!joker.empty()) {
                            if (!CardCombo(joker.begin(), joker.end()).unbe()) ++tmpSmall;
                        }

                        cntSmall = tmpSmall, cntC = tmpC, value = tmpval;
                    }
                }
    }
};

CardCombo lastValidCombo;

struct History {
    Stage stage;
    vector<Card> pub; int llPos, f1Pos, f2Pos;
    vector<CardCombo> playedCombos[3];
    vector<int> bids;
    int numOfCards[3];
    History() {numOfCards[0] = 17, numOfCards[1] = 17, numOfCards[2] = 17; stage = Stage::BIDDING;}
} hist;

struct Player {
    vector<Card> myCards;
    int myPos;
    int cnt[MAX_LEVEL + 1], cntlev;

    Player() {memset(cnt, 0, sizeof cnt);}
    void gain(Card c) {myCards.push_back(c); ++cnt[c2l(c)];}

    template <typename T>
    void erase(T begin, T end) {
        set<Card> S = set<Card>(begin, end);
        for (int i = 0; i < myCards.size(); ++i)
            if (S.count(myCards[i]) > 0) --cnt[c2l(myCards[i])], myCards[i] = 80;
        sort(myCards.begin(), myCards.end());
        while (!myCards.empty() && myCards.back() == 80) myCards.pop_back();
    }

    int minopnum() {
        if (myPos == hist.llPos)
            return (hist.numOfCards[hist.f1Pos] < hist.numOfCards[hist.f2Pos] ? hist.numOfCards[hist.f1Pos] : hist.numOfCards[hist.f2Pos]);
        return hist.numOfCards[hist.llPos];
    }

    CardCombo action(const CardCombo &lt, int stat) {
//        std::cout << hist.llPos << ' ' << hist.numOfCards[hist.llPos] << '\n';
        CardComboType ltType = lt.comboType;
        if (ltType == CardComboType::PASS) {
            if (CardCombo(myCards.begin(), myCards.end()).comboType != CardComboType::INVALID) return CardCombo(myCards.begin(), myCards.end());
            ComboSet origin = ComboSet(myCards.begin(), myCards.end());
            double opval = -120.0, opcntC = 30.0; CardCombo optim;
            unsigned par1[] = {1, 2, 3, 4, 4, 5, 5, 6, 7, 8, 9, 10, 11, 12, 6, 8, 10, 12, 14, 16, 18, 20, 6, 9, 12, 15, 18, 8, 10, 12, 15, 16, 20, 20, 12, 18, 16, 8, 12, 16, 6, 8};
            int par2[] = {1, 1, 1, 1, 1, 1, 5, 6, 7, 8, 9, 10, 11, 12, 3, 4, 5, 6, 7, 8, 9, 10, 2, 3, 4, 5, 6, 2, 2, 3, 3, 4, 4, 5, 2, 3, 2, 2, 3, 4, 1, 1};
            int par3[] = {1, 2, 3, 4, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
            int par4[] = {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 3, 3, 4, 4, 5, 4, 6, 4, 0, 0, 0 ,2, 2};
            int par5[] = {0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 1, 2, 1, 1, 1, 2, 0, 0, 0, 1, 2};
            CardComboType par6[] = {
                CardComboType::SINGLE, CardComboType::PAIR, CardComboType::TRIPLET, CardComboType::BOMB, CardComboType::TRIPLET1, CardComboType::TRIPLET2,
                CardComboType::STRAIGHT, CardComboType::STRAIGHT, CardComboType::STRAIGHT, CardComboType::STRAIGHT, CardComboType::STRAIGHT, CardComboType::STRAIGHT, CardComboType::STRAIGHT, CardComboType::STRAIGHT,
                CardComboType::STRAIGHT2, CardComboType::STRAIGHT2, CardComboType::STRAIGHT2, CardComboType::STRAIGHT2, CardComboType::STRAIGHT2, CardComboType::STRAIGHT2, CardComboType::STRAIGHT2, CardComboType::STRAIGHT2,
                CardComboType::PLANE, CardComboType::PLANE, CardComboType::PLANE, CardComboType::PLANE, CardComboType::PLANE, CardComboType::PLANE1, CardComboType::PLANE2, CardComboType::PLANE1, CardComboType::PLANE2, CardComboType::PLANE1, CardComboType::PLANE2, CardComboType::PLANE1,
                CardComboType::SSHUTTLE2, CardComboType::SSHUTTLE2, CardComboType::SSHUTTLE4, CardComboType::SSHUTTLE, CardComboType::SSHUTTLE, CardComboType::SSHUTTLE, CardComboType::QUADRUPLE2, CardComboType::QUADRUPLE4
            };

            cntlev = 0; for (int i = 0; i < MAX_LEVEL; ++i) if (cnt[i]) ++cntlev;
            for (int ind = 0; ind < 42; ++ind) {
                if (myCards.size() < par1[ind]) continue;
                if (cntlev < par2[ind] + par4[ind]) continue;
                bool maxIsStr =
                    par6[ind] == CardComboType::STRAIGHT || par6[ind] == CardComboType::STRAIGHT2 ||
                    par6[ind] == CardComboType::PLANE || par6[ind] == CardComboType::PLANE1 || par6[ind] == CardComboType::PLANE2 ||
                    par6[ind] == CardComboType::SSHUTTLE || par6[ind] == CardComboType::SSHUTTLE2 || par6[ind] == CardComboType::SSHUTTLE4;
                int cntmain = par2[ind];
                for (Level lvl = 0; ; ++lvl) {
                    int flag = 0;
                    for (int j = 0; j < cntmain; ++j) {
                        int lev = lvl + cntmain - 1 - j;
                        if (
                            (par6[ind] == CardComboType::SINGLE && lev >= MAX_LEVEL) ||
                            (par6[ind] != CardComboType::SINGLE && !maxIsStr && lev >= level_joker) ||
                            (maxIsStr && lev >= MAX_STRAIGHT_LEVEL)
                        ) {flag = 2; break;}
                        if (cnt[lev] < par3[ind]) {flag = 1; break;}
                    }
                    if (flag == 1) continue;
                    else if (flag == 2) break;
                    vector<Card> aux, chos, rem;
                    int reqcnt[MAX_LEVEL + 1] = {0}, auxcnt = par4[ind];
                    for (int j = 0; j < cntmain; ++j) reqcnt[cntmain - j + lvl - 1] = par3[ind];
                    if (auxcnt == 0) {
                        for (int i = 0; i < myCards.size(); ++i) {
                            if (reqcnt[c2l(myCards[i])]) chos.push_back(myCards[i]), --reqcnt[c2l(myCards[i])];
                            else rem.push_back(myCards[i]);
                        }
                        CardCombo chCombo = CardCombo(chos.begin(), chos.end());
                        if (chCombo.comboType == CardComboType::INVALID) continue;
                        ComboSet remSet = ComboSet(rem.begin(), rem.end());
                        double tmpval = remSet.value, tmpcntC = remSet.cntC;

                        if (chCombo.unbe() && (remSet.cntSmall <= 1 || CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID)) tmpval += 150.0;
                        if (stat == 2 && hist.numOfCards[hist.f2Pos] == 1 && chos.size() == 1 && c2l(chos.back()) <= 3) tmpval += 100.0;
                        tmpcntC -= (double)chos.size() * 0.15;
                        if (stat == 0 && chos.size() <= 3 && (chos.size() == hist.numOfCards[hist.f1Pos] || chos.size() == hist.numOfCards[hist.f2Pos])) {
                            tmpval -= 100.0; if (chos.size() == 1 || chos.size() == 2) tmpval += (double)c2l(chos.back()) * 2.0;
                        }
                        if ((stat == 2 || stat == 3) && chos.size() <= 3 && chos.size() == hist.numOfCards[hist.llPos]) {
//                            std::cout << chos.size() << ' ' << c2l(chos.front()) + 3 << ' ' << tmpval << '\n';
                            tmpval -= 100.0; if (chos.size() == 1 || chos.size() == 2) tmpval += (double)c2l(chos.back()) * 2.0;
                        }
                        if ((opval - opcntC * 5.0) < (tmpval - tmpcntC * 5.0)) opval = tmpval, opcntC = tmpcntC, optim = chCombo;
                    } else if (auxcnt > 0) {
                        for (int k = 0; k < MAX_LEVEL; ++k) {
                            if (reqcnt[k] || cnt[k] < par5[ind]) continue;
                            aux.push_back(k);
                        }
                        if (aux.size() < auxcnt) continue;
                        vector<int> use;
                        for (int i = 0; i < auxcnt; ++i) use.push_back(1);
                        for (int i = 0; i < aux.size() - auxcnt; ++i) use.push_back(0);
                        do {
                            chos.clear(); rem.clear(); memset(reqcnt, 0, sizeof reqcnt);
                            for (int j = 0; j < cntmain; ++j) reqcnt[cntmain - j + lvl - 1] = par3[ind];
                            bool flag = false;
                            for (int i = 0; i < aux.size(); ++i) {
                                reqcnt[aux[i]] = par5[ind] * use[i];
                                if (reqcnt[aux[i]] > cnt[aux[i]]) flag = true;
                            }
                            if (flag) continue;

                            for (int i = 0; i < myCards.size(); ++i) {
                                if (reqcnt[c2l(myCards[i])] > 0) chos.push_back(myCards[i]), --reqcnt[c2l(myCards[i])];
                                else rem.push_back(myCards[i]);
                            }
                            CardCombo chCombo = CardCombo(chos.begin(), chos.end());
                            if (chCombo.comboType == CardComboType::INVALID) continue;
                            ComboSet remSet = ComboSet(rem.begin(), rem.end());
                            double tmpval = remSet.value, tmpcntC = remSet.cntC;

                            if (chCombo.unbe() && (remSet.cntSmall <= 1 || CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID)) tmpval += 150.0;
                            tmpcntC -= (double)chos.size() * 0.15;
                            if (stat == 0 && chos.size() <= 3 && (chos.size() == hist.numOfCards[hist.f1Pos] || chos.size() == hist.numOfCards[hist.f2Pos])) {
                                tmpval -= 100.0; if (chos.size() == 1 || chos.size() == 2) tmpval += (double)c2l(chos.back()) * 2.0;
                            }
                            if ((stat == 2 || stat == 3) && chos.size() <= 3 && chos.size() == hist.numOfCards[hist.llPos]) {
                                tmpval -= 100.0; if (chos.size() == 1 || chos.size() == 2) tmpval += (double)c2l(chos.back()) * 2.0;
                            }
                            if ((opval - opcntC * 5.0) < (tmpval - tmpcntC * 5.0)) opval = tmpval, opcntC = tmpcntC, optim = chCombo;
                        } while (next_permutation(use.begin(), use.end()));
                    }
                }
            }
            for (Level i = 0; i < level_joker; ++i) {
                if (cnt[i] == 4) {
                    Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
                    vector<Card> rem;
                    for (int j = 0; j < myCards.size(); ++j)
                        if (myCards[j] / 4 != i) rem.push_back(myCards[j]);

                    vector<Level> oppobomb;
                    for (Level j = 0; j < level_joker; ++j)
                        if (oppocnt[j] == 4) oppobomb.push_back(j);
                    bool mayDie = false;
                    for (int k = 0; k < oppobomb.size(); ++k) if (oppobomb[k] > i) mayDie = true;

                    if (rem.empty() || (CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID && !mayDie))
                        return CardCombo(bomb, bomb + 4);
                }
            }
            if (cnt[level_joker] + cnt[level_JOKER] == 2) {
                Card rocket[] = {card_joker, card_JOKER};
                vector<Card> rem;
                for (int i = 0; i < myCards.size(); ++i) if (myCards[i] < card_joker) rem.push_back(myCards[i]);

                if (rem.empty() || (CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID))
                    return CardCombo(rocket, rocket + 2);
            }
            return optim;
        } else {
            ComboSet origin = ComboSet(myCards.begin(), myCards.end());
            double opval = origin.value - 7.0, opcntC = origin.cntC; CardCombo optim = CardCombo();
//            std::cout << opval << ' ' << opcntC << '\n';
            cntlev = 0; for (int i = 0; i < MAX_LEVEL; ++i) if (cnt[i]) ++cntlev;
            if ((stat == 1 || stat == 4 || stat == 8 || stat == 5 || stat == 7 || stat == 6) && (minopnum() <= 2))
                opval -= 22.0;
            if (ltType == CardComboType::ROCKET) return optim;
            if (myCards.size() >= lt.cards.size() && cntlev >= lt.packs.size()) {
                int cntmain = lt.findMaxSeq();
                bool maxIsStr =
                    ltType == CardComboType::STRAIGHT || ltType == CardComboType::STRAIGHT2 ||
                    ltType == CardComboType::PLANE || ltType == CardComboType::PLANE1 || ltType == CardComboType::PLANE2 ||
                    ltType == CardComboType::SSHUTTLE || ltType == CardComboType::SSHUTTLE2 || ltType == CardComboType::SSHUTTLE4;
                for (Level lvl = 1; ; ++lvl) {
                    int flag = 0;
                    for (int j = 0; j < cntmain; ++j) {
                        int lev = lt.packs[j].level + lvl;
                        if (
                            (ltType == CardComboType::SINGLE && lev >= MAX_LEVEL) ||
                            (ltType != CardComboType::SINGLE && !maxIsStr && lev >= level_joker) ||
                            (maxIsStr && lev >= MAX_STRAIGHT_LEVEL)
                        ) {flag = 2; break;}
                        if (cnt[lev] < lt.packs[j].count) {flag = 1; break;}
                    }
                    if (flag == 1) continue;
                    else if (flag == 2) break;
                    vector<Card> aux, chos, rem;
                    int reqcnt[MAX_LEVEL + 1] = {0}, auxcnt = lt.packs.size() - cntmain;
                    for (int j = 0; j < cntmain; ++j) reqcnt[lt.packs[j].level + lvl] = lt.packs[j].count;

                    if (auxcnt == 0) {
                        for (int i = 0; i < myCards.size(); ++i) {
                            if (reqcnt[c2l(myCards[i])]) chos.push_back(myCards[i]), --reqcnt[c2l(myCards[i])];
                            else rem.push_back(myCards[i]);
                        }
                        CardCombo chCombo = CardCombo(chos.begin(), chos.end());
                        if (chCombo.comboType == CardComboType::INVALID) continue;
                        if (rem.empty()) return chCombo;

                        ComboSet remSet = ComboSet(rem.begin(), rem.end());
                        double tmpval = remSet.value, tmpcntC = remSet.cntC;

                        if (chCombo.unbe() && (remSet.cntSmall <= 1 || CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID)) tmpval += 150.0;
                        if (stat == 6 || stat == 7 || stat == 8)
                            if (chos.size() == hist.numOfCards[hist.llPos])
                                tmpval += ((double)c2l(chos.back()) - 11.0);
                        if (stat == 5 || stat == 6 || stat == 7)
                            tmpval -= (double)(lt.comboLevel + chCombo.comboLevel - 11);
                        if ((opval - opcntC * 5.0) < (tmpval - tmpcntC * 5.0)) opval = tmpval, opcntC = tmpcntC, optim = chCombo; //std::cout << "here\n";
                    } else if (auxcnt > 0) {
                        for (int k = 0; k < MAX_LEVEL; ++k) {
                            if (reqcnt[k] || cnt[k] < lt.packs[cntmain].count) continue;
                            aux.push_back(k);
                        }
                        if (aux.size() < auxcnt) continue;
                        vector<int> use;
                        for (int i = 0; i < auxcnt; ++i) use.push_back(1);
                        for (int i = 0; i < aux.size() - auxcnt; ++i) use.push_back(0);
                        do {
                            chos.clear(); rem.clear(); memset(reqcnt, 0, sizeof reqcnt);
                            for (int j = 0; j < cntmain; ++j) reqcnt[lt.packs[j].level + lvl] = lt.packs[j].count;
                            for (int i = 0; i < aux.size(); ++i) reqcnt[aux[i]] = lt.packs[cntmain].count * use[i];

                            for (int i = 0; i < myCards.size(); ++i) {
                                if (reqcnt[c2l(myCards[i])]) chos.push_back(myCards[i]), --reqcnt[c2l(myCards[i])];
                                else rem.push_back(myCards[i]);
                            }
                            CardCombo chCombo = CardCombo(chos.begin(), chos.end());
                            if (chCombo.comboType == CardComboType::INVALID) continue;
                            if (rem.empty()) return chCombo;

                            ComboSet remSet = ComboSet(rem.begin(), rem.end());
                            double tmpval = remSet.value, tmpcntC = remSet.cntC;

                            if (chCombo.unbe() && (remSet.cntSmall <= 1 || CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID)) tmpval += 150.0;
                            if (stat == 5 || stat == 6 || stat == 7) tmpval -= (double)(lt.comboLevel + chCombo.comboLevel - 11);
                            if (stat == 6 || stat == 7 || stat == 8)
                                if (chos.size() == hist.numOfCards[hist.llPos])
                                    tmpval += ((double)c2l(chos.back()) - 11.0);
                            if ((opval - opcntC * 5.0) < (tmpval - tmpcntC * 5.0)) opval = tmpval, opcntC = tmpcntC, optim = chCombo;
                        } while (next_permutation(use.begin(), use.end()));
                    }
                }
            }
            for (Level i = 0; i < level_joker; ++i) {
                if (cnt[i] == 4 && (ltType != CardComboType::BOMB || i > lt.packs[0].level)) {
                    Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
                    vector<Card> rem;
                    for (int j = 0; j < myCards.size(); ++j)
                        if (myCards[j] / 4 != i) rem.push_back(myCards[j]);
                    if (rem.empty() || CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID)
                        return CardCombo(bomb, bomb + 4);

                    ComboSet remSet = ComboSet(rem.begin(), rem.end());
                    double tmpval = remSet.value, tmpcntC = remSet.cntC;

                    if (stat == 6 || stat == 7 || stat == 8)
                        if (hist.numOfCards[hist.llPos] == lt.cards.size()) tmpval += 12.0;
                    if (stat == 5 || stat == 6 || stat == 7)
                        tmpval -= 20.0;
                    if ((opval - opcntC * 5.0) < (tmpval - tmpcntC * 5.0)) opval = tmpval, opcntC = tmpcntC, optim = CardCombo(bomb, bomb + 4);
                }
            }
            if (cnt[level_joker] + cnt[level_JOKER] == 2) {
                Card rocket[] = {card_joker, card_JOKER};
                vector<Card> rem;
                for (int j = 0; j < myCards.size(); ++j)
                    if (myCards[j] < card_joker) rem.push_back(myCards[j]);
                if (rem.empty() || CardCombo(rem.begin(), rem.end()).comboType != CardComboType::INVALID)
                    return CardCombo(rocket, rocket + 2);

                ComboSet remSet = ComboSet(rem.begin(), rem.end());
                double tmpval = remSet.value, tmpcntC = remSet.cntC;
                if (stat == 1 && (hist.numOfCards[hist.f1Pos] <= 2 || hist.numOfCards[hist.f2Pos] <= 2)) tmpval += 100.0;
                if (stat == 6 || stat == 7 || stat == 8)
                    if (hist.numOfCards[hist.llPos] == lt.cards.size()) tmpval += 15.0;
                if (stat == 5 || stat == 6 || stat == 7)
                    tmpval -= 25.0;
                if ((opval - opcntC * 5.0) < (tmpval - tmpcntC * 5.0)) opval = tmpval, opcntC = tmpcntC, optim = CardCombo(rocket, rocket + 2);
            }
            return optim;
        }
    }
} player;

void cntoppo() {
    for (int i = 0; i < level_joker; ++i) oppocnt[i] = 4;
    oppocnt[level_JOKER] = 1;
    oppocnt[level_joker] = 1;
    for (int i = 0; i < 3; ++i)
        for (unsigned j = 0; j < hist.playedCombos[i].size(); ++j)
            for (unsigned k = 0; k < hist.playedCombos[i][j].cards.size(); ++k)
                oppocnt[c2l(hist.playedCombos[i][j].cards[k])]--;

    for (unsigned i = 0; i < player.myCards.size(); ++i) {
        oppocnt[c2l(player.myCards[i])]--;
    }
}

int status() {
    if (player.myPos == hist.llPos) {
        if (
            hist.playedCombos[hist.f1Pos].back().comboType == CardComboType::PASS &&
            hist.playedCombos[hist.f2Pos].back().comboType == CardComboType::PASS
        ) return 0;
        return 1;
    }
    if (player.myPos == hist.f1Pos) {
        if (
            hist.playedCombos[hist.f2Pos].back().comboType == CardComboType::PASS &&
            hist.playedCombos[hist.llPos].back().comboType == CardComboType::PASS
        ) return 2;
        if (
            hist.playedCombos[hist.llPos].back().comboType == CardComboType::PASS
        ) return 5;
        return 4;
    }
    if (player.myPos == hist.f2Pos) {
        if (
            hist.playedCombos[hist.f1Pos].back().comboType == CardComboType::PASS &&
            hist.playedCombos[hist.llPos].back().comboType == CardComboType::PASS
        ) return 3;
        if (hist.playedCombos[hist.f1Pos].back().comboType == CardComboType::PASS) return 8;
        if (
            hist.playedCombos[hist.f1Pos].size() > 1 &&
            hist.playedCombos[hist.llPos].back().comboType == CardComboType::PASS &&
            hist.playedCombos[hist.f2Pos].back().comboType == CardComboType::PASS
        ) return 6;
        return 7;
    }
    return -1;
}

namespace BotzoneIO {
    using namespace std;
    void read() {
        string line;
        getline(cin, line);
        Json::Value input;
        Json::Reader reader;
        reader.parse(line, input);
        {
            auto firstRequest = input["requests"][0u];
            auto own = firstRequest["own"];
            for (unsigned i = 0; i < own.size(); i++)
                player.gain(own[i].asInt());
            if (!firstRequest["bid"].isNull()) {
                auto bidHistory = firstRequest["bid"];
                player.myPos = bidHistory.size();
                for (unsigned i = 0; i < bidHistory.size(); i++)
                    hist.bids.push_back(bidHistory[i].asInt());
            }
        }

        int turn = input["requests"].size();
        for (int i = 0; i < turn; ++i) {
            auto request = input["requests"][i];
            auto llpublic = request["publiccard"];
            if (!llpublic.isNull()) {
                hist.llPos = request["landlord"].asInt();
//                landlordBid = request["finalbid"].asInt();
                player.myPos = request["pos"].asInt();
                hist.numOfCards[hist.llPos] += llpublic.size();
                for (unsigned i = 0; i < llpublic.size(); i++) {
                    hist.pub.push_back(llpublic[i].asInt());
                    if (hist.llPos == player.myPos)
                        player.gain(llpublic[i].asInt());
                }
            }

            int whoInHistory[] = {(player.myPos + 1) % 3, (player.myPos + 2) % 3};
            auto history = request["history"];
            if (history.isNull()) continue;
            hist.stage = Stage::PLAYING;

            int cntPass = 0;
            for (int p = 0; p < 2; ++p) {
                int pl = whoInHistory[p];
                auto playerAction = history[p];
                vector<Card> playedCards;
                for (unsigned _ = 0; _ < playerAction.size(); _++) {
                    int card = playerAction[_].asInt();
                    playedCards.push_back(card);
                }
                hist.playedCombos[pl].push_back(CardCombo(playedCards.begin(), playedCards.end()));
//                if (pl == hist.llPos) cout << hist.numOfCards[pl] << '\n';
                hist.numOfCards[pl] -= playerAction.size();


                if (playerAction.size() == 0) ++cntPass;
                else lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
            }

            if (cntPass == 2) lastValidCombo = CardCombo();

            if (i < turn - 1) {
                auto playerAction = input["responses"][i];
                vector<Card> playedCards;
                for (unsigned _ = 0; _ < playerAction.size(); _++) {
                    int card = playerAction[_].asInt();
                    playedCards.push_back(card);
                }
                hist.playedCombos[player.myPos].push_back(CardCombo(playedCards.begin(), playedCards.end()));
                hist.numOfCards[player.myPos] -= playerAction.size();
                player.erase(playedCards.begin(), playedCards.end());
            }
        }
        hist.f1Pos = (hist.llPos + 1) % 3, hist.f2Pos = (hist.llPos + 2) % 3;
        cntoppo();
    }

    void bid(int value)
    {
        Json::Value result;
        result["response"] = value;

        Json::FastWriter writer;
        cout << writer.write(result) << endl;
    }

    template <typename CARD_ITERATOR>
    void play(CARD_ITERATOR begin, CARD_ITERATOR end)
    {
        Json::Value result, response(Json::arrayValue);
        for (; begin != end; begin++)
            response.append(*begin);
        result["response"] = response;

        Json::FastWriter writer;
        cout << writer.write(result) << endl;
    }
}

int main() {
    srand(time(nullptr));
    BotzoneIO::read();

    if (hist.stage == Stage::BIDDING) {
        int maxbid = -1;
        for (int i = 0; i < hist.bids.size(); ++i) maxbid = (maxbid < hist.bids[i] ? hist.bids[i] : maxbid);
        ComboSet S = ComboSet(player.myCards.begin(), player.myCards.end());
        double v = S.value - S.cntC * 5.0;
        if (v < -55.0) BotzoneIO::bid(0);
        else if (v < -40.0) BotzoneIO::bid(maxbid >= 1 ? 0 : 1);
        else if (v < -25.0) BotzoneIO::bid(maxbid >= 2 ? 0 : 2);
        else BotzoneIO::bid(3);
    }
    else if (hist.stage == Stage::PLAYING) {
        CardCombo myAction = player.action(lastValidCombo, status());
        BotzoneIO::play(myAction.cards.begin(), myAction.cards.end());
    }
    return 0;
}