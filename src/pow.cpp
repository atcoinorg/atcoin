// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2024-2025 The W-DEVELOP developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/check.h>

unsigned int GetNextWorkRequired(const CBlockIndex *pIndexLast,
                                 const CBlockHeader *pBlock,
                                 const Consensus::Params &params) {
    assert(pIndexLast != nullptr);

    const arith_uint256 powLimit = UintToArith256(params.powLimit);

    const int64_t height = pIndexLast->nHeight;

    const int64_t T = params.nPowTargetSpacing; // 90

    const int64_t N = params.nLWMAAveragingWindow; // 9

    // New coins just "give away" first N blocks. It's better to guess
    // this value instead of using powLimit, but err on high side to not get stuck.
    if ((height + 1) < N) {
        return powLimit.GetCompact();
    }

    if (height < params.switchLWMAblock) {
        // Original Bitcion PoW.
        return BitcoinGetNextWorkRequired(pIndexLast, pBlock, params);
    }

    arith_uint256 prevTarget;
    prevTarget.SetCompact(pIndexLast->nBits);

    const arith_uint256 easingTarget = prevTarget * 6 / 5;
    const arith_uint256 tighteningTarget = prevTarget * 2 / 3;

    // Define a k that will be used to get a proper average after weighting the solvetimes.
    const int64_t k = N * (N + 1) * T / 2;

    const CBlockIndex *blockPreviousTimestamp = pIndexLast->GetAncestor(height - N + 1);
    int64_t previousTimestamp = blockPreviousTimestamp->GetBlockTime();

    arith_uint256 sumTarget;

    int64_t weight = 1;
    // Loop through N most recent blocks.
    for (int64_t i = height - N + 1; i <= height; i++, weight++) {
        const CBlockIndex *block = pIndexLast->GetAncestor(i);

        const int64_t thisTimestamp = (block->GetBlockTime() > previousTimestamp)
                                          ? block->GetBlockTime()
                                          : previousTimestamp + 1;

        int64_t solveTime = thisTimestamp - previousTimestamp;
        const int64_t minSolveTime = T / 6;
        const int64_t maxSolveTime = 6 * T;

        if (solveTime < minSolveTime) {
            solveTime = minSolveTime;
        } else if (solveTime > maxSolveTime) {
            solveTime = maxSolveTime;
        }

        previousTimestamp = thisTimestamp;

        arith_uint256 target;
        target.SetCompact(block->nBits);

        sumTarget += target * solveTime * weight;
    }

    arith_uint256 nextTarget = sumTarget / k;

    if (nextTarget > powLimit) {
        nextTarget = powLimit;
    } else if (nextTarget > easingTarget) {
        nextTarget = easingTarget;
    } else if (nextTarget < tighteningTarget) {
        nextTarget = tighteningTarget;
    }

    return nextTarget.GetCompact();
}

unsigned int BitcoinGetNextWorkRequired(const CBlockIndex *pindexLast, const CBlockHeader *pblock,
                                        const Consensus::Params &params) {
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight + 1) % params.DifficultyAdjustmentInterval() != 0) {
        if (params.fPowAllowMinDifficultyBlocks) {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2) {
                return nProofOfWorkLimit;
            }

            // Return the last non-special-min-difficulty-rules-block
            const CBlockIndex *pindex = pindexLast;
            while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits ==
                   nProofOfWorkLimit)
                pindex = pindex->pprev;
            return pindex->nBits;
        }

        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    const int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval() - 1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex *pindexLast, int64_t nFirstBlockTime,
                                       const Consensus::Params &params) {
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4)
        nActualTimespan = params.nPowTargetTimespan / 4;
    if (nActualTimespan > params.nPowTargetTimespan * 4)
        nActualTimespan = params.nPowTargetTimespan * 4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;

    // Special difficulty rule for Testnet4
    if (params.enforce_BIP94) {
        // Here we use the first block of the difficulty period. This way
        // the real difficulty is always preserved in the first block as
        // it is not allowed to use the min-difficulty exception.
        int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval() - 1);
        const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
        bnNew.SetCompact(pindexFirst->nBits);
    } else {
        bnNew.SetCompact(pindexLast->nBits);
    }

    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

// Check that on difficulty adjustments, the new difficulty does not increase
// or decrease beyond the permitted limits.
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits,
                                   uint32_t new_nbits) {
    if (params.fPowAllowMinDifficultyBlocks) return true;

    if (height % params.DifficultyAdjustmentInterval() == 0) {
        int64_t smallest_timespan = params.nPowTargetTimespan / 4;
        int64_t largest_timespan = params.nPowTargetTimespan * 4;

        const arith_uint256 pow_limit = UintToArith256(params.powLimit);
        arith_uint256 observed_new_target;
        observed_new_target.SetCompact(new_nbits);

        // Calculate the largest difficulty value possible:
        arith_uint256 largest_difficulty_target;
        largest_difficulty_target.SetCompact(old_nbits);
        largest_difficulty_target *= largest_timespan;
        largest_difficulty_target /= params.nPowTargetTimespan;

        if (largest_difficulty_target > pow_limit) {
            largest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 maximum_new_target;
        maximum_new_target.SetCompact(largest_difficulty_target.GetCompact());
        if (maximum_new_target < observed_new_target) return false;

        // Calculate the smallest difficulty value possible:
        arith_uint256 smallest_difficulty_target;
        smallest_difficulty_target.SetCompact(old_nbits);
        smallest_difficulty_target *= smallest_timespan;
        smallest_difficulty_target /= params.nPowTargetTimespan;

        if (smallest_difficulty_target > pow_limit) {
            smallest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 minimum_new_target;
        minimum_new_target.SetCompact(smallest_difficulty_target.GetCompact());
        if (minimum_new_target > observed_new_target) return false;
    } else if (old_nbits != new_nbits) {
        return false;
    }
    return true;
}

// Bypasses the actual proof of work check during fuzz testing with a simplified validation checking whether
// the most significant bit of the last byte of the hash is set.
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params) {
    if constexpr (G_FUZZING) return (hash.data()[31] & 0x80) == 0;
    return CheckProofOfWorkImpl(hash, nBits, params);
}

std::optional<arith_uint256> DeriveTarget(unsigned int nBits, const uint256 pow_limit) {
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(pow_limit))
        return {};

    return bnTarget;
}

bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params& params) {
    auto bnTarget{DeriveTarget(nBits, params.powLimit)};
    if (!bnTarget) return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget) {
        return false;
    }

    return true;
}
