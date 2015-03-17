#ifndef BITCOIN_NCCTRIE_H
#define BITCOIN_NCCTRIE_H

#include "amount.h"
#include "serialize.h"
#include "coins.h"
#include "hash.h"
#include "uint256.h"
#include "util.h"
#include "leveldbwrapper.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include "json/json_spirit_value.h"

#define DEFAULT_DELAY 100

class CNodeValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    CAmount nAmount;
    int nHeight;
    int nValidAtHeight;
    CNodeValue() {};
    //CNodeValue(uint256 txhash, uint32_t nOut) : txhash(txhash), nOut(nOut), nAmount(0), nHeight(0), nValidAtHeight(0) {}
    CNodeValue(uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) : txhash(txhash), nOut(nOut), nAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight) {}
    std::string ToString();
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(nOut);
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(nValidAtHeight);
    }
    
    bool operator<(const CNodeValue& other) const
    {
        if (nAmount < other.nAmount)
            return true;
        else if (nAmount == other.nAmount)
        {
            if (nHeight > other.nHeight)
                return true;
            else if (nHeight == other.nHeight)
            {
                if (txhash.GetHex() > other.txhash.GetHex())
                    return true;
                else if (txhash == other.txhash)
                    if (nOut > other.nOut)
                        return true;
            }
        }
        return false;
    }
    bool operator==(const CNodeValue& other) const
    {
        return txhash == other.txhash && nOut == other.nOut && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
    }
    bool operator!=(const CNodeValue& other) const
    {
        return !(*this == other);
    }
};

class CNCCTrieNode;
class CNCCTrie;

typedef std::map<unsigned char, CNCCTrieNode*> nodeMapType;

class CNCCTrieNode
{
public:
    CNCCTrieNode() {}
    CNCCTrieNode(uint256 hash) : hash(hash) {}
    uint256 hash;
    uint256 bestBlock;
    nodeMapType children;
    std::vector<CNodeValue> values;
    bool insertValue(CNodeValue val, bool * fChanged = NULL);
    bool removeValue(uint256& txhash, uint32_t nOut, CNodeValue& val, bool * fChanged = NULL);
    bool getBestValue(CNodeValue& val) const;
    bool empty() const {return children.empty() && values.empty();}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(values);
    }
    
    bool operator==(const CNCCTrieNode& other) const
    {
        return hash == other.hash && values == other.values;
    }

    bool operator!=(const CNCCTrieNode& other) const
    {
        return !(*this == other);
    }
private:
    bool getValue(uint256& txhash, uint32_t nOut, CNodeValue& val) const; 
};

struct nodenamecompare
{
    bool operator() (const std::string& i, const std::string& j) const
    {
        if (i.size() == j.size())
            return i < j;
        return i.size() < j.size();
    }
};

class CValueQueueEntry
{
    public:
    CValueQueueEntry() {}
    CValueQueueEntry(std::string name, CNodeValue val) : name(name), val(val) {}
    std::string name;
    CNodeValue val;
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(name);
        READWRITE(val);
    }
};

typedef std::map<int, std::vector<CValueQueueEntry> > valueQueueType;
typedef std::vector<CValueQueueEntry> CNCCTrieQueueUndo;

typedef std::map<std::string, CNCCTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

class CNCCTrieCache;

class CNCCTrie
{
public:
    CNCCTrie() : db(GetDataDir() / "ncctrie", 100, false, false), root(uint256S("0000000000000000000000000000000000000000000000000000000000000001")) {}
    uint256 getMerkleHash();
    CLevelDBWrapper db;
    bool empty() const;
    bool checkConsistency();
    bool ReadFromDisk(bool check = false);
    json_spirit::Array dumpToJSON() const;
    bool getInfoForName(const std::string& name, CNodeValue& val) const;
    int nCurrentHeight;
    bool queueEmpty() const;
    friend class CNCCTrieCache;
private:
    bool update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlock, valueQueueType& queueCache, int nNewHeight);
    bool updateName(const std::string& name, CNCCTrieNode* updatedNode, std::vector<std::string>& deletedNames, CNCCTrieNode** pNodeRet);
    bool updateHash(const std::string& name, uint256& hash, CNCCTrieNode** pNodeRet);
    bool recursiveNullify(CNCCTrieNode* node, std::string& name, std::vector<std::string>& deletedNames);
    bool recursiveCheckConsistency(CNCCTrieNode* node);
    bool BatchWrite(nodeCacheType& changedNodes, std::vector<std::string>& deletedNames, const uint256& hashBlock, std::vector<int> vChangedQueueRows, std::vector<int> vDeletedQueueRows, int nNewHeight);
    bool InsertFromDisk(const std::string& name, CNCCTrieNode* node);
    bool recursiveDumpToJSON(const std::string& name, const CNCCTrieNode* current, json_spirit::Array& ret) const;
    CNCCTrieNode root;
    uint256 hashBlock;
    valueQueueType valueQueue;
    valueQueueType::iterator getQueueRow(int nHeight);
    void deleteQueueRow(int nHeight);
    void BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CNCCTrieNode* pNode) const;
    void BatchEraseNode(CLevelDBBatch& batch, const std::string& nome) const;
    void BatchWriteQueueRow(CLevelDBBatch& batch, int nRowNum);
    void BatchEraseQueueRow(CLevelDBBatch& batch, int nRowNum);
};

class CNCCTrieCache
{
public:
    CNCCTrieCache(CNCCTrie* base): base(base), nCurrentHeight(base->nCurrentHeight) {}
    uint256 getMerkleHash() const;
    bool empty() const;
    bool flush();
    bool dirty() const { return !dirtyHashes.empty(); }
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) const;
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, uint256 prevTxhash, uint32_t nPrevOut) const;
    bool undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const;
    bool spendClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const;
    uint256 getBestBlock();
    void setBestBlock(const uint256& hashBlock);
    bool incrementBlock(CNCCTrieQueueUndo& undo) const;
    bool decrementBlock(CNCCTrieQueueUndo& undo) const;
    ~CNCCTrieCache() { clear(); }
    bool insertClaimIntoTrie(const std::string name, CNodeValue val) const;
    bool removeClaimFromTrie(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight) const;
private:
    CNCCTrie* base;
    bool getInfoForName(const std::string name, CNodeValue& val) const;
    mutable nodeCacheType cache;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    mutable valueQueueType valueQueueCache;
    mutable int nCurrentHeight;
    uint256 computeHash() const;
    bool recursiveComputeMerkleHash(CNCCTrieNode* tnCurrent, std::string sPos) const;
    bool recursivePruneName(CNCCTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified = NULL) const;
    bool clear() const;
    bool removeClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool addClaimToQueue(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const;
    bool removeClaimFromQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeightToCheck, int& nValidAtHeight) const;
    valueQueueType::iterator getQueueCacheRow(int nHeight, bool createIfNotExists) const;
    uint256 hashBlock;
};

#endif // BITCOIN_NCCTRIE_H
