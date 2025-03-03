#include <string>
#include <map>
#include <iomanip>
#include <iostream>
#include <algorithm>

using namespace std;

#include "seq_info.h"
#include "utils.h"
#include "seq_utils.h"
#include "sequence.h"
#include "seq_reader.h"

// for each character in the alphabet 'seq_chars_'
void SeqInfo::count_chars_indiv_seq(string& seq) {
    seq = string_to_upper(seq);
    total_.clear(); // probably unnecessary
    for (unsigned int i = 0; i < seq_chars_.length(); i++) {
        total_[seq_chars_[i]] = 0.0;
    }
    for (unsigned int i = 0; i < seq.length(); i++) {
        // Ensure there is no weird J or whatever characters (includes '?')
        if (total_.find(seq[i]) == total_.end()) {
            total_[missing_]++;
        } else {
            total_[seq[i]]++;
        }
    }
}

// alternate to above, accumulate char counts across seqs
void SeqInfo::count_chars (string& seq) {
    unsigned int sum = 0;
    seq = string_to_upper(seq);
    if (output_indiv_) {
        vector <int> icounts(seq_chars_.length(), 0);
        for (unsigned int i = 0; i < seq_chars_.length(); i++) {
            int num = count(seq.begin(), seq.end(), seq_chars_[i]);
            char_counts_[i] += num;
            icounts[i] += num;
            sum += num;
        }
        // add invalid char counts, add to missing char count
        if (sum < seq.length()) {
            char_counts_[char_counts_.size() - 1] += (seq.length() - sum);
            icounts[icounts.size() - 1] += (seq.length() - sum);
        }
        indiv_char_counts_.push_back(icounts);
        // this is cool, but unnecessary here
        //std::transform(char_counts_.begin(), char_counts_.end(), icounts.begin(), char_counts_.begin(), std::plus<int>());
    } else {
        for (unsigned int i = 0; i < seq_chars_.length(); i++) {
            int num = count(seq.begin(), seq.end(), seq_chars_[i]);
            char_counts_[i] += num;
            sum += num;
        }
        // add invalid char counts, add to missing char count
        if (sum < seq.length()) {
            char_counts_[char_counts_.size() - 1] += (seq.length() - sum);
        }
    }
}

// calculate character state frequencies
void SeqInfo::calculate_freqs () {
    seqcount_ = 0;
    bool first = true;
    Sequence seq;
    vector <Sequence> seqs; // needed for interleaved nexus, or non-nexus multi
    string retstring;
    int ft = test_seq_filetype_stream(*pios_, retstring);
    file_type_ = get_filetype_string(ft);
    
    // if nexus, a bunch of metadata are available
    // this is mostly useful with morphology
    // data may also be interleaved
    // so treat separately
    if (file_type_.compare("nexus") == 0) {
        int nexus_ntax = 0; // compare to num read to find issues
        bool is_interleaved = false;
        get_nexus_alignment_properties (*pios_, nexus_ntax, seq_length_,
                is_interleaved, alpha_name_, seq_chars_, gap_, missing_);
        //cout << "alpha_name_ = " << alpha_name_ << endl;
        set_datatype();
        if (is_multi_ && seq_chars_.compare("") != 0) {
            seq_chars_ += gap_;
            seq_chars_ += missing_;
            alpha_set_ = true;
        }
        retstring = ""; // have to set so seq_reader knows we are mid-file
        if (!is_interleaved) {
            while (read_next_seq_from_stream(*pios_, ft, retstring, seq)) {
                if (first) {
                    if (!datatype_set_) {
                        alpha_name_ = seq.get_alpha_name();
                        cout << "am i here?" << endl;
                        set_datatype();
                    }
                    if (!alpha_set_) {
                        // i believe this is dead
                        set_alphabet();
                        //cout << endl << "i am going to use alphabet: " << seq_chars_ << endl << endl;
                    } else {
                        //cout << "already got alphabet: " << seq_chars_ << endl;
                        // set in nexus. dead code
                        char_counts_.resize(seq_chars_.size(), 0);
                    }
                    first = false;
                }
                seqcount_++;
                temp_seq_ = seq.get_sequence();
                concatenated_ += temp_seq_;
                name_ = seq.get_id();
                seq_lengths_.push_back(temp_seq_.length());
                count_chars(temp_seq_);
                taxon_labels_.push_back(name_);
            }
        } else {
            // need to read in everything at once
            seqs = read_interleaved_nexus (*pios_, nexus_ntax, seq_length_);
            for (unsigned int i = 0; i < seqs.size(); i++) {
                seq = seqs[i];
                seqcount_++;
                temp_seq_ = seq.get_sequence();
                concatenated_ += temp_seq_;
                name_ = seq.get_id();
                seq_lengths_.push_back(temp_seq_.length());
                count_chars(temp_seq_);
                taxon_labels_.push_back(name_);
            }
        }
        if (nexus_ntax != seqcount_) {
            cout << "badly formatted nexus file: " << seqcount_ << " sequences read but "
                    << nexus_ntax << " expected. Exiting." <<  endl;
            exit(1);
        }
    } else {
        while (read_next_seq_from_stream(*pios_, ft, retstring, seq)) {
            if (first) {
                if (!datatype_set_) { // only if forced protein
                    alpha_name_ = seq.get_alpha_name();
                    set_datatype();
                }
                //cout << "alpha_name_ = " << alpha_name_ << endl;
                first = false;
            }
            if (!is_multi_) {
                seqcount_++;
                concatenated_ += seq.get_sequence();
                temp_seq_ = seq.get_sequence();
                name_ = seq.get_id();
                seq_lengths_.push_back(temp_seq_.length());
                count_chars(temp_seq_);
                taxon_labels_.push_back(name_);
            } else {
                seqs.push_back(seq);
                concatenated_ += seq.get_sequence();
            }
        }
        if (ft == 2) {
            if (!is_multi_) {
                seqcount_++;
                concatenated_ += seq.get_sequence();
                temp_seq_ = seq.get_sequence();
                name_ = seq.get_id();
                seq_lengths_.push_back(temp_seq_.length());
                count_chars(temp_seq_);
                taxon_labels_.push_back(name_);
            } else {
                seqs.push_back(seq);
                concatenated_ += seq.get_sequence();
            }
        }
        // figure out alphabet from entire alignment
        if (is_multi_) {
            // grab all unique characters from the input string
            // here, seqs from all individuals are concatenated, so represents all sampled characters
            set_alphabet_from_sampled_seqs(concatenated_);
            // now, do the character counting as above
            for (unsigned int i = 0; i < seqs.size(); i++) {
                seq = seqs[i];
                seqcount_++;
                temp_seq_ = seq.get_sequence();
                name_ = seq.get_id();
                seq_lengths_.push_back(temp_seq_.length());
                count_chars(temp_seq_);
                taxon_labels_.push_back(name_);
            }
        }
    }
}

// alt to print_summary_table_whole_alignment. essential difference is transposed results
void SeqInfo::return_freq_table (ostream* poos) {
    const char separator = ' ';
    const int colWidth = 10;
    if (output_indiv_) {
        // need to take into account longest_tax_label_
        get_longest_taxon_label();
        string pad = std::string(longest_tax_label_, ' ');
        // header
        (*poos) << pad << " ";
        for (unsigned int i = 0; i < seq_chars_.length(); i++) {
            (*poos) << right << setw(colWidth) << setfill(separator)
                << seq_chars_[i] << " ";
        }
        // return nchar for individual seqs
        (*poos) << right << setw(colWidth) << setfill(separator) << "Nchar" << endl;
        for (int i = 0; i < seqcount_; i++) {
            int diff = longest_tax_label_ - taxon_labels_[i].size();
            (*poos_) << taxon_labels_[i];
            if (diff > 0) {
                pad = std::string(diff, ' ');
                (*poos_) << pad;
            }
            (*poos_) << " ";
            for (unsigned int j = 0; j < seq_chars_.length(); j++) {
                (*poos) << right << setw(colWidth) << setfill(separator)
                    << (double)indiv_char_counts_[i][j] / (double)seq_lengths_[i] << " ";
            }
            (*poos) << right << setw(colWidth) << setfill(separator) << seq_lengths_[i] << endl;
        }
    } else {
        // header
        for (unsigned int i = 0; i < seq_chars_.length(); i++) {
            (*poos) << right << setw(colWidth) << setfill(separator)
                << seq_chars_[i];
            if (i != seq_chars_.length() - 1) {
                (*poos) << " ";
            }
        }
        (*poos) << endl;
        // counts
        for (unsigned int i = 0; i < seq_chars_.length(); i++) {
            (*poos) << right << setw(colWidth) << setfill(separator)
                << char_counts_[i];
            if (i != seq_chars_.length() - 1) {
                (*poos) << " ";
            }
        }
        (*poos) << endl;
        // freqs
        int total_num_chars = sum(char_counts_);
        for (unsigned int i = 0; i < seq_chars_.length(); i++) {
            (*poos) << fixed << right << setw(colWidth) << setfill(separator)
                << (double)char_counts_[i] / (double)total_num_chars;
            if (i != seq_chars_.length() - 1) {
                (*poos) << " ";
            }
        }
        (*poos) << endl;
    }
}

void SeqInfo::print_summary_table_whole_alignment (ostream* poos) {
    const char separator = ' ';
    const int colWidth = 10;
    double total_num_chars = 0.0;
    
    //(*poos) << "General Stats For All Sequences" << endl;
    (*poos) << "File type: " << file_type_ << endl;
    (*poos) << "Number of sequences: " << seqcount_ << endl;
    if (std::adjacent_find( seq_lengths_.begin(), seq_lengths_.end(), std::not_equal_to<int>()) == seq_lengths_.end() ) {
        is_aligned_ = true;
    } else {
        is_aligned_ = false;
    }
    (*poos_) << "Is aligned: " << std::boolalpha << is_aligned_ << endl;
    if (is_aligned_) {
        seq_length_ = seq_lengths_[0];
        (*poos_) << "Sequence length: " << seq_length_ << endl;
        total_num_chars = (double)(seq_lengths_[0] * seqcount_);
    } else {
        total_num_chars = (double)sum(seq_lengths_);
    }
    
    (*poos) << "--------" << seq_type_ << " TABLE---------" << endl;
    (*poos) << left << setw(6) << setfill(separator) << seq_type_ << " "
        << setw(colWidth) << setfill(separator) << "Total" << " "
        << setw(colWidth) << setfill(separator) << "Proportion" << endl;
    for (unsigned int i = 0; i < seq_chars_.length(); i++) {
        (*poos) << left << setw(6) << setfill(separator) << seq_chars_[i] << " "
            << setw(colWidth) << setfill(separator) << total_[seq_chars_[i]] << " "
            << ((total_[seq_chars_[i]] / total_num_chars)) << endl;
    }
    if (is_dna_) {
        (*poos) << left << setw(6) << setfill(separator) << "G+C" << " "
            << setw(colWidth) << setfill(separator) << (total_['G'] + total_['C']) << " "
            << (((total_['G'] + total_['C']) / total_num_chars)) << endl;
    }
    (*poos) << "--------" << seq_type_ << " TABLE---------" << endl;
}

// just grab labels, disregard the rest
void SeqInfo::collect_taxon_labels () {
    Sequence seq;
    string retstring;
    int ft = test_seq_filetype_stream(*pios_, retstring);
    while (read_next_seq_from_stream(*pios_, ft, retstring, seq)) {
        name_ = seq.get_id();
        taxon_labels_.push_back(name_);
    }
    if (ft == 2) {
        name_ = seq.get_id();
        taxon_labels_.push_back(name_);
    }
    sort(taxon_labels_.begin(), taxon_labels_.end());
}

// assumed aligned if all seqs are the same length
void SeqInfo::check_is_aligned () {
    Sequence seq;
    string retstring;
    int ft = test_seq_filetype_stream(*pios_, retstring);
    while (read_next_seq_from_stream(*pios_, ft, retstring, seq)) {
        int terp = seq.get_sequence().size();
        seq_lengths_.push_back(terp);
    }
    if (ft == 2) {
        int terp = seq.get_sequence().size();
        seq_lengths_.push_back(terp);
    }
    // check if all seqs are the same length
    if (std::adjacent_find( seq_lengths_.begin(), seq_lengths_.end(), std::not_equal_to<int>()) == seq_lengths_.end() ) {
        is_aligned_ = true;
    } else {
        is_aligned_ = false;
    }
}

void SeqInfo::get_nseqs () {
    seqcount_ = 0;
    bool is_interleaved = false;
    Sequence seq;
    string retstring;
    int ft = test_seq_filetype_stream(*pios_, retstring);
    
    // if nexus, grab from dimensions; no need to read alignment
    if (ft == 0) {
        get_nexus_dimensions (*pios_, seqcount_, seq_length_, is_interleaved);
    } else {
        while (read_next_seq_from_stream(*pios_, ft, retstring, seq)) {
            seqcount_++;
        }
        if (ft == 2) {
            seqcount_++;
        }
    }
}

void SeqInfo::get_nchars () {
    Sequence seq;
    string retstring;
    bool is_interleaved = false;
    int ft = test_seq_filetype_stream(*pios_, retstring);
    
    // if nexus, grab from dimensions; no need to read alignment (i.e., trusting file is valid)
    if (ft == 0) {
        get_nexus_dimensions (*pios_, seqcount_, seq_length_, is_interleaved);
    } else {
        while (read_next_seq_from_stream(*pios_, ft, retstring, seq)) {
            int terp = seq.get_sequence().size();
            name_ = seq.get_id();
            taxon_labels_.push_back(name_);
            seq_lengths_.push_back(terp);
        }
        if (ft == 2) {
            int terp = seq.get_sequence().size();
            name_ = seq.get_id();
            taxon_labels_.push_back(name_);
            seq_lengths_.push_back(terp);
        }
        // check if all seqs are the same length
        if (std::adjacent_find( seq_lengths_.begin(), seq_lengths_.end(), std::not_equal_to<int>()) == seq_lengths_.end() ) {
            is_aligned_ = true;
            seq_length_ = seq_lengths_[0];
        } else {
            is_aligned_ = false;
            seq_length_ = -1;
        }
        seqcount_ = (int)seq_lengths_.size();
    }
}

// does not currently do per-individual...
// assumes gap=- and missing=?
void SeqInfo::calc_missing () {
    calculate_freqs();
    // missing data are the last two characters (-N for DNA, -X for protein)
    int total_num_chars = sum(char_counts_);
    
    int miss = 0;
    double temp = 0.0;
    for (unsigned int i = seq_chars_.length()-2; i < seq_chars_.length(); i++) {
        temp += (double)char_counts_[i] / (double)total_num_chars;
        miss += char_counts_[i];
    }
    percent_missing_ = temp;
    
    cout << "total_num_chars = " << total_num_chars << endl;
    cout << "total missing = " << miss << endl;
}

// get the longest label. for printing purposes
void SeqInfo::get_longest_taxon_label () {
    longest_tax_label_ = 0;
    for (int i = 0; i < seqcount_; i++) {
        if ((int)taxon_labels_[i].size() > longest_tax_label_) {
            longest_tax_label_ = taxon_labels_[i].size();
        }
    }
}

SeqInfo::SeqInfo (istream* pios, ostream* poos, bool& indiv,
        bool const& force_protein):seq_chars_(""), output_indiv_(indiv), datatype_set_(false),
        is_dna_(false), is_protein_(false), is_multi_(false), is_binary_(false),
        alpha_set_(false), alpha_name_(""), seq_type_(""), gap_('-'), missing_('?') {
    // maybe get rid of this? how often is inference wrong?
    if (force_protein) {
        is_protein_ = true;
        datatype_set_ = true;
        set_alphabet();
    }
    pios_ = pios;
    poos_ = poos;
}

// return whichever property set to true
void SeqInfo::get_property (bool const& get_labels, bool const& check_aligned,
        bool const& get_nseq, bool const& get_freqs, bool const& get_nchar,
        double const& get_missing) {
    
    if (get_labels) {
        collect_taxon_labels();
        for (unsigned int i = 0; i < taxon_labels_.size(); i++) {
            (*poos_) << taxon_labels_[i] << endl;
        }
    } else if (check_aligned) {
        check_is_aligned();
        (*poos_) << std::boolalpha << is_aligned_ << endl;
    } else if (get_nseq) {
        get_nseqs();
        (*poos_) << seqcount_ << endl;
    } else if (get_freqs) {
        calculate_freqs();
        return_freq_table(poos_);
    } else if (get_missing) {
        calc_missing();
        (*poos_) << percent_missing_ << endl;
    } else if (get_nchar) {
        get_nchars ();
        if (!output_indiv_) { // single return value
            if (seq_length_ != -1) {
                (*poos_) << seq_length_ << endl;
            } else {
                // not aligned
                (*poos_) << "sequences are not aligned" << endl;
            }
        } else { // individual lengths
            get_longest_taxon_label();
            for (int i = 0; i < seqcount_; i++) {
                int diff = longest_tax_label_ - taxon_labels_[i].size();
                (*poos_) << taxon_labels_[i];
                if (diff > 0) {
                    string pad = std::string(diff, ' ');
                    (*poos_) << pad;
                }
                (*poos_) << " " << seq_lengths_[i] << endl;
            }
        }
    }
}

void SeqInfo::set_datatype () {
    if (alpha_name_ == "DNA") {
        is_dna_ = true;
        seq_type_ = "Nucl";
        set_alphabet();
    } else if (alpha_name_ == "AA") {
        is_protein_ = true;
        seq_type_ = "Prot";
        set_alphabet();
    } else if (alpha_name_ == "BINARY") {
        is_binary_ = true;
        seq_type_ = "Binary";
        set_alphabet();
    } else if (alpha_name_ == "MULTI") {
        is_multi_ = true;
        seq_type_ = "Multi";
        // alphabet is either 1) supplied (nexus) or 2) comes from entire alignment
    } else {
        cout << "Don't know what kind of alignment this is :( Exiting." << endl;
        exit(0);
    }
    datatype_set_ = true;
}

// TODO: need to add morphology ('MULTI') data
// - may be passed in (nexus)
// - otherwise, needs to be gleaned from entire alignment
void SeqInfo::set_alphabet () {
    if (is_protein_) {
        seq_chars_ = "ACDEFGHIKLMNPQRSTVWYX";
    } else if (is_binary_) {
        seq_chars_ = "01";
    } else {
        seq_chars_ = "ACGT"; // not using ambiguity codes here
    }
    seq_chars_ += gap_;
    seq_chars_ += missing_;
    char_counts_.resize(seq_chars_.size(), 0);
    alpha_set_ = true;
}

// TODO: get rid of the concatenated_ business
void SeqInfo::summarize () {
    // a concatenated seq (i.e., across all indiv) will be used for all stats
    calculate_freqs();
    
    if (output_indiv_) {
        // new one
        return_freq_table(poos_);
    } else {
        // pass the seq concatenated across all individuals
        // probably can skip a bunch fo the stuff above...
        count_chars_indiv_seq(concatenated_);
        print_summary_table_whole_alignment(poos_);
    }
}

void SeqInfo::set_alphabet_from_sampled_seqs (string const& seq) {
    seq_chars_ = get_alphabet_from_sequence(seq);
    // expecting order: valid, gap, missing
    // remove gap and missing (if present)
    seq_chars_.erase(std::remove(seq_chars_.begin(), seq_chars_.end(), gap_), seq_chars_.end());
    seq_chars_.erase(std::remove(seq_chars_.begin(), seq_chars_.end(), missing_), seq_chars_.end());
    
    // and append them back on
    seq_chars_ += gap_;
    seq_chars_ += missing_;
    char_counts_.resize(seq_chars_.size(), 0);
    alpha_set_ = true;
    
}
