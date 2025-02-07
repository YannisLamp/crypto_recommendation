#ifndef CRYPTO_REC_HPP
#define CRYPTO_REC_HPP

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <ctime>

#include "./data_structures/cust_vector.hpp"
#include "./data_structures/tweet.h"
#include "lsh_cube.hpp"


/*
 * Different essential functions used for cryptocurrency recommendation
 *
 * This file also contains the declarations and definitions for functions that needed to be templated
 *
 */


// Create and return different CustVector objects, each representing a user, from an input tweet map
template <typename dim_type>
std::vector< CustVector<dim_type> > tweets_to_user_vectors(std::unordered_map<std::string, Tweet>& tweets, int crypto_num);

// Create and return different CustVector objects, each representing a user, from an input vectors that have been clustered
template <typename dim_type>
std::vector< CustVector<dim_type> > clusters_to_user_vectors(std::unordered_map<std::string, Tweet>& tweets,
        std::vector< CustVector<dim_type> >& vectors, int crypto_num, int user_num);

// Returns a vector parallel to the input neighbors vector that contains the cosine similarity of each user pair
// The neighbors vector is sorted and filtered in this function
template <typename dim_type>
std::vector<double> get_P_closest(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user, int P);

// Parralel quicksort implementation
template <typename dim_type, typename type>
void parallel_quickSort(std::vector<dim_type>& sim, std::vector< type >& neighbors, int low, int high);

// Parallel partition implementation, to be used in parallel quicksort for cosine similarities and neighbors
template <typename dim_type, typename type>
int parralel_partition(std::vector<dim_type>& sim, std::vector< type >& neighbors, int low, int high);

// For a user, calculate and return his predicted scores for unknown cryptocurrencies
template <typename dim_type>
std::vector<dim_type> get_predicted_user_sim(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user,
        std::vector<double> similarities);

// Return predicted top N (highest scoring) cryptocurrency indexes for an input user
template <typename dim_type>
std::vector<int> get_top_N_recom(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user, int N,
        std::vector<double> similarities);

// Return predicted top N (highest scoring) cryptocurrency indexes for an input user
template <typename dim_type>
std::vector<int> get_top_N_recom(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user, int N);

// Split vector of Custom vector into 10 vectors, used for 10-fold cross validation
template <typename dim_type>
std::vector< std::vector< CustVector<dim_type> > > split_to_10(std::vector< CustVector<dim_type> > input_vectors);

// Merge input vectors into a vector of Custom vectors except for one, used for 10-fold cross validation
template <typename dim_type>
std::vector< CustVector<dim_type> > merge_except_for(std::vector< std::vector< CustVector<dim_type> > > vectors_to_merge,
        int not_merge_index);

// Alter input Custom vector so that one random score is hidden (returned by the function)
template <typename dim_type>
bool hide_one_score(CustVector<dim_type>& inVector, double* old_score);

/*
* Template utility function definitions
*/


template <typename dim_type>
std::vector< CustVector<dim_type> > tweets_to_user_vectors(std::unordered_map<std::string, Tweet>& tweets, int crypto_num) {
    std::unordered_map< std::string, std::vector<dim_type> > user_map;
    std::unordered_map< std::string, std::vector<int> > known_index_map;

    // For each tweet,
    for (auto& tweet : tweets) {
        std::string user_id = tweet.second.getUserId();
        std::vector<int> crypto_indexes = tweet.second.getCryptoIndexes();
        double score = tweet.second.getSentimentScore();

        if (user_map.count(user_id) == 0) {
            std::vector<dim_type> user_vector(crypto_num);
            user_map.emplace(user_id, user_vector);

            std::vector<int> is_unknown(crypto_num);
            known_index_map.emplace(user_id, is_unknown);
        }

        for (auto index : crypto_indexes) {
            if (score > 0)
                user_map[user_id][index] = user_map[user_id][index] + score;

            known_index_map[user_id][index] = 1;
        }
    }

    // Create CustVector objects to represent each user
    std::vector< CustVector<dim_type> > user_vectors;
    for (auto& user : user_map) {
        // Create set of unknown cryptocurrency indexes and calculate vector mean
        double sum = 0;
        int known_number = 0;
        std::set<int> unknown_indexes;
        bool useless = true;
        for (int i = 0; i < user.second.size(); i++) {
            if (known_index_map[user.first][i] == 0) {
                unknown_indexes.emplace(i);
            }
            else {
                sum = sum + user.second[i];
                known_number++;
            }

            if (user.second[i] != 0)
                useless = false;
        }

        // Filter vectors that we know nothing of
        if (!useless) {
            double mean = sum / known_number;

            // Replace unknown cryptocurrency scores with mean
            for (int index : unknown_indexes)
                user.second[index] = mean;

            CustVector<dim_type> current_user(user.first, user.second, unknown_indexes, mean);
            user_vectors.emplace_back(current_user);
        }
    }

    return user_vectors;
}


template <typename dim_type>
std::vector< CustVector<dim_type> > clusters_to_user_vectors(std::unordered_map<std::string, Tweet>& tweets,
        std::vector< CustVector<dim_type> >& vectors, int crypto_num, int user_num) {

    // For intermediate calculations
    std::vector< std::vector<dim_type> > inter_vectors(user_num);
    std::vector< std::vector<int> > known_indexes(user_num);
    for (int i = 0; i < user_num; i++) {
        std::vector<dim_type> user_vector(crypto_num);
        inter_vectors[i] = user_vector;

        std::vector<int> is_unknown(crypto_num);
        known_indexes[i] = is_unknown;
    }

    for (auto& vec : vectors) {
        if (tweets.count(vec.getId()) > 0) {
            std::string tweet_id = vec.getId();
            Tweet currTweet = tweets.at(tweet_id);

            std::vector<int> crypto_indexes = currTweet.getCryptoIndexes();
            double score = currTweet.getSentimentScore();

            for (auto index : crypto_indexes) {
                if (score > 0)
                    inter_vectors[vec.getCluster()][index] = inter_vectors[vec.getCluster()][index] + score;

                known_indexes[vec.getCluster()][index] = 1;
            }
        }
    }

    // Create CustVector objects to represent each user
    std::vector< CustVector<dim_type> > user_vectors;
    for (int user_index = 0; user_index < inter_vectors.size(); user_index++) {
        // Create set of unknown cryptocurrency indexes and calculate vector mean
        double sum = 0;
        int known_number = 0;
        std::set<int> unknown_indexes;
        bool useless = true;
        for (int i = 0; i < inter_vectors[user_index].size(); i++) {
            if (known_indexes[user_index][i] == 0) {
                unknown_indexes.emplace(i);
            }
            else {
                sum = sum + inter_vectors[user_index][i];
                known_number++;
            }

            if (inter_vectors[user_index][i] != 0)
                useless = false;
        }

        // Filter vectors that we know nothing of
        if (!useless) {
            double mean = sum / known_number;

            // Replace unknown cryptocurrency scores with mean
            for (int index : unknown_indexes)
                inter_vectors[user_index][index] = mean;

            CustVector<dim_type> current_user(std::to_string(user_index), inter_vectors[user_index], unknown_indexes, mean);
            user_vectors.emplace_back(current_user);
        }
    }

    return user_vectors;
}


template <typename dim_type>
std::vector<double> get_P_closest(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user, int P) {
    // Cache cosine similarities in vector
    std::vector<double> similarities(neighbors.size());

    // Calculate and store similarities
    for (int i = 0; i < neighbors.size(); i++)
        similarities[i] = neighbors[i]->cosineSimilarity(&user);

    // Parallel quicksort algorithm for the two vectors (neighbors and similarities
    parallel_quickSort(similarities, neighbors, 0, similarities.size()-1);

    if (neighbors.size() > P) {
        neighbors.resize(P);
        similarities.resize(P);
    }

    return similarities;
}


template <typename dim_type, typename type>
int parralel_partition(std::vector<dim_type>& sim, std::vector< type >& neighbors, int low, int high) {
    double pivot = sim[high];
    int i = (low - 1);

    for (int j = low; j <= high- 1; j++) {
        // If current element is greater than or equal to pivot
        if (sim[j] >= pivot) {
            i++;

            // Parallel swap
            dim_type temp_sim = sim[i];
            sim[i] = sim[j];
            sim[j] = temp_sim;

            type temp_neigh = neighbors[i];
            neighbors[i] = neighbors[j];
            neighbors[j] = temp_neigh;
        }
    }

    dim_type temp_sim = sim[i + 1];
    sim[i + 1] = sim[high];
    sim[high] = temp_sim;

    type temp_neigh = neighbors[i + 1];
    neighbors[i + 1] = neighbors[high];
    neighbors[high] = temp_neigh;

    return (i + 1);
}


// Parralel quicksort implementation
template <typename dim_type, typename type>
void parallel_quickSort(std::vector<dim_type>& sim, std::vector< type >& neighbors, int low, int high) {
    if (low < high) {
        int pi = parralel_partition(sim, neighbors, low, high);

        // Separately sort elements before partition and after partition
        parallel_quickSort(sim, neighbors, low, pi - 1);
        parallel_quickSort(sim, neighbors, pi + 1, high);
    }
}


template <typename dim_type>
std::vector<dim_type> get_predicted_user_sim(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user,
        std::vector<double> similarities) {
    std::vector<dim_type> predicted_scores(user.getDimensions()->begin(), user.getDimensions()->end());

    for (int index : user.getUnknownIndexes()) {
        double main_sum = 0;
        double abs_sum = 0;

        for (int i = 0; i < neighbors.size(); i++) {
            double cosine_sim = similarities[i];
            abs_sum = abs_sum + fabs(cosine_sim);

            std::vector<dim_type>* neigh_scores = neighbors[i]->getDimensions();
            double neigh_mean = neighbors[i]->getKnownMean();

            main_sum = main_sum + (cosine_sim * (neigh_scores->at(index) - neigh_mean));
        }

        double predicted_score = main_sum / abs_sum;
        predicted_score = predicted_score + user.getKnownMean();

        predicted_scores[index] = predicted_score;
    }

    return predicted_scores;
}


template <typename dim_type>
std::vector<int> get_top_N_recom(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user, int N,
        std::vector<double> similarities) {

    std::vector<dim_type> predicted_scores = get_predicted_user_sim(neighbors, user, similarities);
    std::vector<int> unknown_indexes = user.getUnknownIndexes();
    std::vector<dim_type> unknown_predicted(unknown_indexes.size());

    for (int i = 0; i < unknown_indexes.size(); i++)
        unknown_predicted[i] = predicted_scores[unknown_indexes[i]];

    parallel_quickSort(unknown_predicted, unknown_indexes, 0, unknown_predicted.size()-1);

    unknown_indexes.resize(N);
    return unknown_indexes;
}


template <typename dim_type>
std::vector<int> get_top_N_recom(std::vector< CustVector<dim_type>* >& neighbors, CustVector<dim_type>& user, int N) {
    // If no similarities as input, calculate from scratch
    std::vector<double> similarities(neighbors.size());
    for (int i = 0; i < neighbors.size(); i++)
        similarities[i] = neighbors[i]->cosineSimilarity(&user);

    std::vector<dim_type> predicted_scores = get_predicted_user_sim(neighbors, user, similarities);
    std::vector<int> unknown_indexes = user.getUnknownIndexes();
    std::vector<dim_type> unknown_predicted(unknown_indexes.size());

    for (int i = 0; i < unknown_indexes.size(); i++)
        unknown_predicted[i] = predicted_scores[unknown_indexes[i]];

    parallel_quickSort(unknown_predicted, unknown_indexes, 0, unknown_predicted.size()-1);

    unknown_indexes.resize(N);
    return unknown_indexes;
}


template <typename dim_type>
std::vector< std::vector< CustVector<dim_type> > > split_to_10(std::vector< CustVector<dim_type> > input_vectors) {
    srand((int)time(0));
    int each_vector_size = input_vectors.size() / 10;

    std::vector< std::vector< CustVector<dim_type> > > split_vectors;
    for (int i = 0; i < 10; i++) {
        std::vector< CustVector<dim_type> > curr_split;
        curr_split.reserve(each_vector_size);
        for (int curr_size = 0; curr_size < each_vector_size && input_vectors.size() > 0; curr_size++) {
            int rand_i = rand() % input_vectors.size();

            curr_split.emplace_back(input_vectors[rand_i]);
            input_vectors.erase(input_vectors.begin() + rand_i);
        }
        split_vectors.emplace_back(curr_split);
    }

    return split_vectors;
}


template <typename dim_type>
std::vector< CustVector<dim_type> > merge_except_for(std::vector< std::vector< CustVector<dim_type> > > vectors_to_merge,
        int not_merge_index) {

    int whole_size = 0;
    for (int i = 0; i < vectors_to_merge.size(); i++) {
        if (i != not_merge_index)
            whole_size = whole_size + vectors_to_merge[i].size();
    }

    std::vector< CustVector<dim_type> > merged;
    merged.reserve(whole_size);

    for (int i = 0; i < vectors_to_merge.size(); i++) {
        if (i != not_merge_index) {
            merged.insert(merged.end(), vectors_to_merge[i].begin(), vectors_to_merge[i].end());
        }
    }

    return merged;
}


template <typename dim_type>
bool hide_one_score(CustVector<dim_type>& inVector, double* old_score) {
    // Find random index to hide
    std::vector<dim_type>& in_dimensions = *(inVector.getDimensions());

    // Get all known indexes
    std::vector<int> known_indexes;
    for (int i = 0; i < in_dimensions.size(); i++) {
        if (inVector.getUnknownIndexesSet().count(i) == 0)
            known_indexes.emplace_back(i);
    }
    // If the user oly knows one cryptocurrency before hiding, then skip hiding process
    if (known_indexes.size() < 2)
        return false;


    // Get random index and save the old score to be hidden
    srand((int)time(0));
    int hide_index = rand() % known_indexes.size();
    *old_score = in_dimensions[hide_index];

    // Now "known" cryptocurrencies will have the value of 0
    for (int i : inVector.getUnknownIndexes())
        in_dimensions[i] = 0;

    // Calculate new mean
    double new_mean = 0;
    int known_num = 0;
    bool useless = true;
    for (int i = 0; i < in_dimensions.size(); i++) {
        if (i != hide_index) {
            new_mean = new_mean + in_dimensions[i];
            known_num++;

            if (in_dimensions[i] != 0)
                useless = false;
        }
    }

    if (useless)
        return false;

    // Make the unknown score's value equal to the new means
    new_mean = new_mean / known_num;
    in_dimensions[hide_index] = new_mean;

    //for (int i : inVector.getUnknownIndexes()) {
    //    new_dims[i] = new_means;
    //}

    std::set<int> new_unknown_set;
    new_unknown_set.emplace(hide_index);
    inVector.setKnownMean(new_mean);
    inVector.setUnknownIndexes(new_unknown_set);

    return true;
}

#endif //CRYPTO_REC_HPP
