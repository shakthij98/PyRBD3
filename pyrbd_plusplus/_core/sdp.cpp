#include <pyrbd_plusplus/sdp.hpp>
#include <pyrbd_plusplus/utils.hpp>
#include <numeric>
#include <chrono>
#include <mutex>
#include <malloc.h>
#include <algorithm>
#include <queue>
#include <iostream>
#include <fstream>
#include <set>

#define DEBUG_OUTPUT 0

#if DEBUG_OUTPUT
    #define DEBUG_COUT std::cout
#else
    #define DEBUG_COUT if(false) std::cout
#endif

namespace pyrbdpp::sdp
{   

    using pyrbdpp::utils::isSubSet;
    using pyrbdpp::utils::hasCommonElement;
    using pyrbdpp::utils::toString;


    SDPSets eliminateSDPSet(SDPSets &sdpSets)
    {   
        DEBUG_COUT << "Eliminating SDP sets: " << toString(sdpSets) << std::endl;

        // Sort the sdp sets, non-complementary sets first, then complementary sets
        std::sort(sdpSets.begin(), sdpSets.end(), [](const SDP &a, const SDP &b) {
            return a.isComplementary() < b.isComplementary();
        });

        // Initialize a vector to store the eliminated elements
        std::vector<NodeID> eliminatedElements;

        // Create a new set for the eliminated SDP sets and reserve space for it
        SDPSets eliminatedSet;
        eliminatedSet.reserve(sdpSets.size());

        // Temporary set to store the new set of elements
        std::vector<int> newSet;

        for (auto &SDP : sdpSets)
        {
            if (!SDP.isComplementary())
            {   
                // If the set is non complementary, we add its elements to the eliminatedElements
                eliminatedElements.insert(eliminatedElements.end(), SDP.begin(), SDP.end());
                std::sort(eliminatedElements.begin(), eliminatedElements.end()); // Ensure the eliminated elements are sorted
                eliminatedSet.push_back(std::move(SDP));

                DEBUG_COUT << "Update to be eliminated elements: " << toString(eliminatedElements) << std::endl;
            }
            else
            {
                // If the set is complementary, check if it has any elements in common with the eliminatedElements
                newSet.clear(); 
                std::set_difference(SDP.begin(), SDP.end(), 
                eliminatedElements.begin(), eliminatedElements.end(),
                                    std::back_inserter(newSet));
                // If the new set is not empty, we add it to the eliminatedSet
                if (!newSet.empty())
                {   
                    DEBUG_COUT << "Adding eliminated SDP: " << toString(newSet) << std::endl;
                    eliminatedSet.emplace_back(true, std::move(newSet));
                }
            }
        }

        return eliminatedSet;
    }


    SDPSets absorbSDPSet(SDPSets sdpSets)
    {   
        // Initialize a vector to store the absorbed SDPs and reserve space for it
        SDPSets absorbedSDPs;
        absorbedSDPs.reserve(sdpSets.size());        

        // Initialize a vector of boolean marks to track absorbed sets
        std::vector<bool> absorbed(sdpSets.size(), false);

        for (size_t i = 0; i < sdpSets.size(); ++i)
        {
            if (absorbed[i])
                continue;

            const auto &currentSDP = sdpSets[i];

            for (size_t j = i + 1; j < sdpSets.size(); ++j)
            {
                if (absorbed[j])
                    continue;

                const auto &otherSDP = sdpSets[j];

                if (currentSDP.equals(otherSDP))
                {
                    absorbed[j] = true; // Mark the duplicate as absorbed
                    continue; // Skip further checks for this pair
                } 
                else if (isSubSet(currentSDP, otherSDP))
                {
                    absorbed[j] = true; // Mark the superset as absorbed

                } else if (isSubSet(otherSDP, currentSDP))
                {
                    absorbed[i] = true; // Mark the subset as absorbed
                    break; // No need to check further for this currentSDP
                }
            }

            if (!absorbed[i])
            {
                absorbedSDPs.push_back(sdpSets[i]); // Dont move the SDP, will be still be used
            }
        }

        return absorbedSDPs;
    }

    std::vector<SDPSets> decomposeSDPSet(SDPSets sdpSets)
    {   
        // Vector to store the results of the decomposition
        std::vector<SDPSets> results;

        // Queue to store the SDP sets for processing
        std::queue<SDPSets> queue;
        queue.push(std::move(sdpSets));

        while (!queue.empty())
        {   
            // Get the current SDP sets from the queue
            SDPSets current = std::move(queue.front());
            queue.pop();

            // Check if the current SDP sets have any common elements
            if (!hasCommonElement(current))
            {   
                results.push_back(std::move(current));
                continue;
            }

            // Separate the complementary sets and non-complementary sets
            SDPSets complementarySDPs, normalSDPs;
            for (auto &SDP : current)
            {
                if (SDP.isComplementary())
                {
                    complementarySDPs.push_back(std::move(SDP));
                }
                else
                {
                    normalSDPs.push_back(std::move(SDP));
                }
            }

            // Save the common SDP sets pair and their common elements
            std::pair<SDP, SDP> commonSDPpair;
            std::vector<NodeID> commonElements;

            // Find only the first common elements in the complementary sets
            bool found = false;

            // Save the index of the two common pairs
            size_t foundI = 0;
            size_t foundJ = 0;
            
            for (size_t i = 0; i < complementarySDPs.size() && !found; ++i)
            {
                for (size_t j = i + 1; j < complementarySDPs.size(); ++j)
                {   
                    commonElements.clear(); // Clear the common elements for each pair
                    // Find the intersection of the two sets
                    std::set_intersection(complementarySDPs[i].begin(), complementarySDPs[i].end(),
                                          complementarySDPs[j].begin(), complementarySDPs[j].end(),
                                          std::back_inserter(commonElements));
                    if (!commonElements.empty())
                    {
                        commonSDPpair = std::make_pair(complementarySDPs[i], complementarySDPs[j]);
                        found = true;
                        foundI = i;
                        foundJ = j;
                        break;
                    }
                }
            }

            // If no common elements are found, add the current SDP sets to the results and continue
            if (!found)
            {   
                results.push_back(std::move(current));
                continue;
            }

            // Add the not common SDP sets to the normalSDPs
            for (size_t i = 0; i < complementarySDPs.size(); ++i)
            {
                if (i != foundI && i != foundJ)
                {
                    normalSDPs.push_back(std::move(complementarySDPs[i]));
                }
            }

            // Remove the common elements from the common pairs
            for (const auto &elem : commonElements)
            {
                commonSDPpair.first.remove(elem);
                commonSDPpair.second.remove(elem);
            }
            // DEBUG
            DEBUG_COUT << "Decomposing Part: " << std::endl;
            DEBUG_COUT << "Normal SDPs: " << toString(normalSDPs) << std::endl;

            // Reserve space for the decomposed SDPs
            SDPSets decomposed1, decomposed2;
            decomposed1.reserve(normalSDPs.size() + 1);
            decomposed2.reserve(normalSDPs.size() + 3);

            // For the first new SDP sets we add all normal SDPs and the complementary SDP with the common elements
            decomposed1 = normalSDPs;
            decomposed1.emplace_back(true, commonElements);

            //DEBUG
            DEBUG_COUT << "Decomposed1: " << toString(decomposed1) << std::endl;

            // For the second new SDP sets we add
            // 1. all normal SDP
            // 2. Non complementary SDP with the common elements
            // 3. Complementary SDP with elements from the first common pair without common elements
            // 4. Complementary SDP with elements from the second common pair without common elements
            decomposed2 = std::move(normalSDPs);
            decomposed2.emplace_back(false, std::move(commonElements));
            decomposed2.push_back(std::move(commonSDPpair.first));
            decomposed2.push_back(std::move(commonSDPpair.second));

            // DEBUG
            DEBUG_COUT << "Decomposed2: " << toString(decomposed2) << std::endl;

            // Eliminate and absorb the decomposed SDPs
            decomposed1 = eliminateSDPSet(decomposed1);
            decomposed2 = eliminateSDPSet(decomposed2);

            // DEBUG
            DEBUG_COUT << "Eliminated Decomposed1: " << toString(decomposed1) << std::endl;
            DEBUG_COUT << "Eliminated Decomposed2: " << toString(decomposed2) << std::endl;
            
            decomposed1 = absorbSDPSet(decomposed1);
            decomposed2 = absorbSDPSet(decomposed2);

            // DEBUG
            DEBUG_COUT << "Absorbed Decomposed1: " << toString(decomposed1) << std::endl;
            DEBUG_COUT << "Absorbed Decomposed2: " << toString(decomposed2) << std::endl;

            // Add it to the queue for further decomposition
            queue.push(std::move(decomposed1));
            queue.push(std::move(decomposed2));
        
        }
        
        return results;

    }

    PathSets sortPathSet(PathSets pathSets)
    {   
        // If the pathSets is empty, return an empty vector
        if (pathSets.empty())
        {
            return {};
        }

        // Initial the result sorted pathSet
        PathSets sortedPathSet;
        sortedPathSet.reserve(pathSets.size());

        // Sort the integers in each set of pathSets in ascending order
        for (auto &set : pathSets)
        {
            std::sort(set.begin(), set.end());
        }

        // Sort the pathSets in ascending order according to the size of the set and the number in each set
        std::sort(pathSets.begin(), pathSets.end(), [](const std::vector<int> &a, const std::vector<int> &b)
                  {
            if (a.size() != b.size())
            {
                return a.size() < b.size();
            }
            return a < b; });

        // Initialize a map to store the pathSets with the size of the pathSet as the key
        std::map<int, PathSets> pathSetMap;
        
        // Save the pathSets in the map
        for (auto &set : pathSets)
        {
            pathSetMap[set.size()].push_back(std::move(set)); // Here we move the set to avoid copying
        }

        // Save the first size of the pathSet to the sortedPathSet as initial value
        auto it = pathSetMap.begin();
        if (it != pathSetMap.end())
        {
            // Move the first set to the sortedPathSet
            std::move(it->second.begin(), it->second.end(), std::back_inserter(sortedPathSet));
            // Add the iterator to the next element
            ++it;
        }

        // Sort the sets with the same size according to the increasing maximal number of literals in common with the preceding sets
        for (; it != pathSetMap.end(); ++it)
        {
            // pair.first is the size of the set, pair.second is the vector of sets with this size
            auto &unsortedSets = it->second;

            // Create a vector to store the maximal number of literals in common with the preceding sets
            Set maxCommonCounts;

            // For each pathset stores the maximal number of literals in common with the preceding sets
            maxCommonCounts.reserve(unsortedSets.size());
            std::transform(unsortedSets.begin(), unsortedSets.end(), std::back_inserter(maxCommonCounts), 
                            [&sortedPathSet](const std::vector<int> &set)
                           {
                int maxCommonNum = 0;
                for (const auto &precedSet : sortedPathSet)
                {
                    // Count the number of literals in common
                    int commonNum = std::count_if(set.begin(), set.end(), [&precedSet](int elem) {
                        return std::find(precedSet.begin(), precedSet.end(), elem) != precedSet.end();
                    });

                    // If the current set has more literals in common, update the best set
                    if (commonNum > maxCommonNum)
                    {
                        maxCommonNum = commonNum;
                    }
                }
                return maxCommonNum; });

            // Create new indices for the unsortedSets based on the maximal number of literals in common in ascending order
            std::vector<size_t> indices(unsortedSets.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&maxCommonCounts](size_t a, size_t b)
                      { return maxCommonCounts[a] < maxCommonCounts[b]; });

            // Add the sorted sets to the sortedPathSet based on the indices
            for (size_t idx : indices)
            {
                sortedPathSet.push_back(std::move(unsortedSets[idx])); // Here we move the set to avoid copying
            }
        }

        // Now we have the sorted pathSet, we can return it
        return sortedPathSet;
    }

    std::vector<SDPSets> toSDPSet(NodeID src, NodeID dst, PathSets pathSets)
    {
        // Sort the pathSets
        PathSets sortedPathSet = sortPathSet(std::move(pathSets));

        // DEBUG
        DEBUG_COUT << "Sorted PathSets: " << std::endl;
        DEBUG_COUT << toString(sortedPathSet) << std::endl;
        

        if (sortedPathSet.empty())
        {
            return {};
        }

        // Initialize a the final SDP sets with the non-complementary first set in the sorted pathSet
        std::vector<SDPSets> finalSDPs = {{{false, sortedPathSet.front()}}};

        // DEBUG
        DEBUG_COUT << "Initial finalSDPs: " << std::endl;
        DEBUG_COUT << toString(finalSDPs) << std::endl;
        DEBUG_COUT << "Sorted PathSets: " << std::endl;
        DEBUG_COUT << toString(sortedPathSet) << std::endl;

        // Iterate over the sorted pathSet starting from the second set
        for (size_t i = 1; i < sortedPathSet.size(); ++i)
        {   
            // Initialize result SDPs for the current set
            SDPSets resultSDPs;

            // Add the current set as a non-complementary SDP in the result
            const auto &currentSet = sortedPathSet[i];
            resultSDPs.emplace_back(false, currentSet);

            // DEBUG
            DEBUG_COUT << "======================================================================================== " << std::endl;
            DEBUG_COUT << "Processing set: " << toString(currentSet) << std::endl;
            DEBUG_COUT << "Current resultSDPs: " << toString(resultSDPs) << std::endl;

            // Iterate over the previous sets in sortedPathSet
            for (size_t j = 0; j < i; ++j)
            {   
                // Create the RC set: elements in precedingSet but not in currentSet
                std::vector<int> RC;
                const auto &precedingSet = sortedPathSet[j]; 
                // DEBUG
                DEBUG_COUT << "Comparing with preceding set: " << toString(precedingSet) << std::endl;
                std::set_difference(precedingSet.begin(), precedingSet.end(),
                                    currentSet.begin(), currentSet.end(),
                                    std::back_inserter(RC));
                if (!RC.empty())
                {   
                    // DEBUG
                    DEBUG_COUT << "Adding RC set: " << toString(RC) << std::endl;
                    resultSDPs.emplace_back(true, std::move(RC));
                }
            }

            // Absorb the resultSDPs to remove any redundant sets
            resultSDPs = absorbSDPSet(std::move(resultSDPs));
            // DEBUG
            DEBUG_COUT << "Absorbed resultSDPs: " << toString(resultSDPs) << std::endl;

            // Decompose the resultSDPs if it has common elements
            if (hasCommonElement(resultSDPs))
            {
                // DEBUG
                DEBUG_COUT << "Decomposing resultSDPs: " << toString(resultSDPs) << std::endl;
                std::vector<SDPSets> decomposedResults = decomposeSDPSet(std::move(resultSDPs));
                // DEBUG
                DEBUG_COUT << "Decomposed results: " << std::endl;
                DEBUG_COUT << toString(decomposedResults) << std::endl;
                std::move(decomposedResults.begin(), decomposedResults.end(), std::back_inserter(finalSDPs));
                // DEBUG
                DEBUG_COUT << "Updated finalSDPs after decomposition: " << std::endl;
                DEBUG_COUT << toString(finalSDPs) << std::endl;
            }
            else
            {
                // If there are no common elements, just add the resultSDPs to the finalSDPs
                finalSDPs.push_back(std::move(resultSDPs));
                // DEBUG
                DEBUG_COUT << "Added resultSDPs to finalSDPs: " << std::endl;
                DEBUG_COUT << toString(finalSDPs) << std::endl;
            }
        }

        return finalSDPs;
    }

    std::vector<SDPSets> toSDPSetParallel(NodeID src, NodeID dst, PathSets pathSets)
    {   
        // Check if the pathSets size is less than 200
        if (pathSets.size() < 200)
        {
            // If the pathSets size is less than 200, use the single-threaded version
            // std::cerr << "Warning: Trying to use multithreaded version of toSDPSet for small pathSets size: " << pathSets.size() << std::endl;
            // std::cerr << "Using single-threaded version instead." << std::endl;
            // Call the single-threaded version of toSDPSet
            return toSDPSet(src, dst, pathSets);
        }
        
        // Sort the pathSets
        PathSets sortedPathSet = sortPathSet(std::move(pathSets));

        // Initialize a the final thread results
        std::vector<std::vector<SDPSets>> threadResults(sortedPathSet.size());

        // Initialize the first thread result with the non-complementary first set in the sorted pathSet
        threadResults[0] = {{{false, sortedPathSet.front()}}};

        // Iterate over the sorted pathSet starting from the second set
        #pragma omp parallel for schedule(dynamic)
        for (size_t i = 1; i < sortedPathSet.size(); ++i)
        {   
            // Save the result for the current SDP
            SDPSets resultSDPs;

            // Initialize the result SDPs with the current set as a non-complementary SDP
            const auto &currentSet = sortedPathSet[i];
            resultSDPs.emplace_back(false, currentSet);

            // Iterate over the previous sets in sortedPathSet
            for (size_t j = 0; j < i; ++j)
            {   
                // Create the RC set: elements in precedingSet but not in currentSet
                std::vector<int> RC;
                const auto &precedingSet = sortedPathSet[j];
                std::set_difference(precedingSet.begin(), precedingSet.end(),
                                    currentSet.begin(), currentSet.end(),
                                    std::back_inserter(RC));
                if (!RC.empty())
                {
                    resultSDPs.emplace_back(true, std::move(RC));
                }
            }

            // Absorb the resultSDPs to remove any redundant sets
            resultSDPs = absorbSDPSet(std::move(resultSDPs));

            // Decompose the resultSDPs if it has common elements
            if (hasCommonElement(resultSDPs))
            {
                threadResults[i] = decomposeSDPSet(std::move(resultSDPs));
            }
            else
            {
                // If there are no common elements, just add the resultSDPs to the threadResults
                threadResults[i] = {std::move(resultSDPs)};
            }
        }

        std::vector<SDPSets> finalSDPs;
        // Combine the results from all threads into the finalSDP
        for (const auto &threadResult : threadResults)
        {
            std::move(threadResult.begin(), threadResult.end(), std::back_inserter(finalSDPs));
        }

        return finalSDPs;
    }

    double SDPSetToAvail(const ProbabilityMap &probaMap, const std::vector<SDPSets> &sdpSets)
    {
        double availability = 0.0;

        // Iterate over each set in the SDP set
        for (const auto &set : sdpSets)
        {
            double setAvailability = 1.0;

            // Compute the availability for the current set
            for (const auto &SDP : set)
            {

                if (SDP.isComplementary())
                {
                    double tmp_avail = 1.0;

                    // If the set is complementary, use the negative probability
                    for (const auto &elem : SDP.getSet())
                    {
                        tmp_avail *= probaMap[elem];
                    }

                    setAvailability *= (1.0 - tmp_avail);
                }
                else
                {
                    // If the set is normal, use the positive probability
                    for (const auto &elem : SDP.getSet())
                    {
                        setAvailability *= probaMap[elem];
                    }
                }
            }

            // Add the set availability to the total availability
            availability += setAvailability;
        }

        return availability;
    }

    double evalAvail(NodeID src, NodeID dst, const ProbabilityMap &probaMap, PathSets &pathSets)
    {
        // Convert the pathSets to the SDP set
        std::vector<SDPSets> SDPs = toSDPSet(src, dst, pathSets);

        // Calculate the availability of the SDP set
        double availability = SDPSetToAvail(probaMap, SDPs);

        return availability;
    }

    double evalAvailParallel(NodeID src, NodeID dst, const ProbabilityMap &probaMap, PathSets &pathSets)
    {
        // Convert the pathSets to the SDP set
        std::vector<SDPSets> SDPs = toSDPSetParallel(src, dst, pathSets);

        // Calculate the availability of the SDP set
        double availability = SDPSetToAvail(probaMap, SDPs);

        return availability;
    }

    std::vector<AvailTriple> evalAvailTopo(const NodePairs &nodePairs, const ProbabilityMap &probaMap, std::vector<PathSets> &pathsetsList)
    {
        std::vector<AvailTriple> availList;

        for (size_t i = 0; i < nodePairs.size(); ++i)
        {   
            const auto &[src, dst] = nodePairs[i];
            auto &pathSets = pathsetsList[i];

            double availability = evalAvail(src, dst, probaMap, pathSets);
            availList.emplace_back(src, dst, availability);
        }

        return availList;
    }

    std::vector<AvailTriple> evalAvailTopoParallel(const NodePairs &nodePairs, const ProbabilityMap &probaMap, std::vector<PathSets> &pathsetsList)
    {
        std::vector<AvailTriple> availList(nodePairs.size());

        #pragma omp parallel for schedule(dynamic)
        for (size_t i = 0; i < nodePairs.size(); ++i)
        {
            const auto &[src, dst] = nodePairs[i];
            auto &pathSets = pathsetsList[i];

            double availability = evalAvail(src, dst, probaMap, pathSets);
            availList[i] = std::make_tuple(src, dst, availability);
        }

        return availList;
    }

} // namespace pyrbdpp::sdp