#ifndef CLAIMTRIE_TRIE_H
#define CLAIMTRIE_TRIE_H

#include <data.h>
#include <sqlite.h>
#include <txoutpoint.h>
#include <uints.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>

CUint256 getValueHash(const CTxOutPoint& outPoint, int nHeightOfLastTakeover);

class CClaimTrie
{
    friend class CClaimTrieCacheBase;
    friend class ClaimTrieChainFixture;
    friend class CClaimTrieCacheHashFork;
    friend class CClaimTrieCacheExpirationFork;
    friend class CClaimTrieCacheNormalizationFork;

public:
    CClaimTrie() = delete;
    CClaimTrie(CClaimTrie&&) = delete;
    CClaimTrie(const CClaimTrie&) = delete;
    CClaimTrie(int64_t cacheBytes, bool fWipe, int height = 0,
               const std::string& dataDir = ".",
               int nNormalizedNameForkHeight = 1,
               int64_t nOriginalClaimExpirationTime = 1,
               int64_t nExtendedClaimExpirationTime = 1,
               int64_t nExtendedClaimExpirationForkHeight = 1,
               int64_t nAllClaimsInMerkleForkHeight = 1,
               int proportionalDelayFactor = 32);

    CClaimTrie& operator=(CClaimTrie&&) = delete;
    CClaimTrie& operator=(const CClaimTrie&) = delete;

    bool empty();
    bool SyncToDisk();

protected:
    int nNextHeight;
    const std::string dbFile;
    sqlite::database db;
    const int nProportionalDelayFactor;

    const int nNormalizedNameForkHeight;
    const int64_t nOriginalClaimExpirationTime;
    const int64_t nExtendedClaimExpirationTime;
    const int64_t nExtendedClaimExpirationForkHeight;
    const int64_t nAllClaimsInMerkleForkHeight;
};

class CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheBase(CClaimTrie* base);
    virtual ~CClaimTrieCacheBase();

    bool flush();
    bool checkConsistency();
    CUint256 getMerkleHash();
    bool validateDb(int height, const CUint256& rootHash);

    std::size_t getTotalNamesInTrie() const;
    std::size_t getTotalClaimsInTrie() const;
    int64_t getTotalValueOfClaimsInTrie(bool fControllingOnly) const;

    bool haveClaim(const std::string& name, const CTxOutPoint& outPoint) const;
    bool haveClaimInQueue(const std::string& name, const CTxOutPoint& outPoint, int& nValidAtHeight) const;

    bool haveSupport(const std::string& name, const CTxOutPoint& outPoint) const;
    bool haveSupportInQueue(const std::string& name, const CTxOutPoint& outPoint, int& nValidAtHeight) const;

    bool addClaim(const std::string& name, const CTxOutPoint& outPoint, const CUint160& claimId, int64_t nAmount,
                  int nHeight, int nValidHeight = -1, const std::vector<unsigned char>& metadata = {});

    bool addSupport(const std::string& name, const CTxOutPoint& outPoint, const CUint160& supportedClaimId, int64_t nAmount,
                    int nHeight, int nValidHeight = -1, const std::vector<unsigned char>& metadata = {});

    bool removeClaim(const CUint160& claimId, const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight);
    bool removeSupport(const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight);

    virtual bool incrementBlock();
    virtual bool decrementBlock();
    virtual bool finalizeDecrement();

    virtual int expirationTime() const;

    virtual bool getProofForName(const std::string& name, const CUint160& claim, CClaimTrieProof& proof);
    virtual bool getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset = 0) const;

    virtual CClaimSupportToName getClaimsForName(const std::string& name) const;
    virtual std::string adjustNameForValidHeight(const std::string& name, int validHeight) const;

    void getNamesInTrie(std::function<void(const std::string&)> callback) const;
    bool getLastTakeoverForName(const std::string& name, CUint160& claimId, int& takeoverHeight) const;
    bool findNameForClaim(std::vector<unsigned char> claim, CClaimValue& value, std::string& name) const;

protected:
    int nNextHeight; // Height of the block that is being worked on, which is
    CClaimTrie* base;
    mutable sqlite::database db;
    mutable std::unordered_set<std::string> removalWorkaround;

    mutable sqlite::database_binder claimHashQuery, childHashQuery, claimHashQueryLimit;

    virtual CUint256 computeNodeHash(const std::string& name, int takeoverHeight);
    supportEntryType getSupportsForName(const std::string& name) const;

    virtual int getDelayForName(const std::string& name, const CUint160& claimId) const;

    bool deleteNodeIfPossible(const std::string& name, std::string& parent, int64_t& claims);
    void ensureTreeStructureIsUpToDate();
    void ensureTransacting();

private:
    bool transacting;
    // for unit test
    friend struct ClaimTrieChainFixture;
    friend class CClaimTrieCacheTest;

    bool activateAllFor(const std::string& name);
};

#endif // CLAIMTRIE_TRIE_H