/*
 * main_mrca.cpp
 *
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <getopt.h>

using namespace std;

#include "node.h"
#include "tree_reader.h"
#include "string_node_object.h"
#include "vector_node_object.h"
#include "tree.h"
#include "utils.h"
#include "bd_sim.h"
#include "log.h"

void print_help() {
    cout << "Get the number of descendant tips of internal nodes specified by mrca statements." << endl;
    cout << "Takes in newick tree and MRCA file with format:" << endl;
    cout << "MRCANAME = tip1 tip2 ..." << endl;
    cout << endl;
    cout << "Usage: pxmrca [OPTION]... " << endl;
    cout << endl;
    cout << " -t, --treef=FILE    input newick tree file, stdin otherwise" << endl;
    cout << " -o, --outf=FILE     output newick file, stout otherwise" << endl;
    cout << " -m, --mrca=FILE     file containing MRCA declarations" << endl;
    cout << " -h, --help          display this help and exit" << endl;
    cout << " -V, --version       display version and exit" << endl;
    cout << endl;
    cout << "Report bugs to: <https://github.com/FePhyFoFum/phyx/issues>" << endl;
    cout << "phyx home page: <https://github.com/FePhyFoFum/phyx>" << endl;
}
string versionline("pxmrca 0.1\nCopyright (C) 2013 FePhyFoFum\nLicense GPLv3\nwritten by Stephen A. Smith (blackrim)");

static struct option const long_options[] =
{
    {"treef", required_argument, NULL, 't'},
    {"outf", required_argument, NULL, 'o'},
    {"mrca", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char * argv[]) {
    
    log_call(argc, argv);
    
    char * outf = NULL;
    char * treef = NULL;
    char * mrcaf = NULL;
    bool outfileset = false;
    bool fileset = false;
    bool mrcaset = false;
    
    while (1) {
        int oi = -1;
        int c = getopt_long(argc, argv, "t:o:m:hV", long_options, &oi);
        if (c == -1) {
            break;
        }
        switch(c) {
            case 't':
                fileset = true;
                treef = strdup(optarg);
                check_file_exists(treef);
                break;
            case 'o':
                outfileset = true;
                outf = strdup(optarg);
                break;
            case 'm':
                mrcaset = true;
                mrcaf = strdup(optarg);
                break;
            case 'h':
                print_help();
                exit(0);
            case 'V':
                cout << versionline << endl;
                exit(0);
            default:
                print_error(argv[0], (char)c);
                exit(0);
        }
    }
    
    if (fileset && outfileset) {
        check_inout_streams_identical(treef, outf);
    }
    
    if (!mrcaset) {
        cout << "Must supply mrca file" << endl;
        exit(0);
    }
    
    istream * pios = NULL;
    ostream * poos = NULL;
    ifstream * fstr = NULL;
    ofstream * ofstr = NULL;
    
    if (outfileset == true) {
        ofstr = new ofstream(outf, ios::app);
        poos = ofstr;
    } else {
        poos = &cout;
    }
    if (fileset == true) {
        fstr = new ifstream(treef);
        pios = fstr;
    } else {
        pios = &cin;
        if (check_for_input_to_stream() == false) {
            print_help();
            exit(1);
        }
    }
    
    ifstream inmrca(mrcaf);
    string mrcaline;
    map<string, vector<string> > mrcas;
    while (getline(inmrca, mrcaline)) {
        if (mrcaline.empty()) {
            continue;
        }
        vector<string> searchtokens;
        tokenize(mrcaline, searchtokens, "=");
        string mrcaname = searchtokens[0];
        trim_spaces(mrcaname);
        searchtokens.erase(searchtokens.begin());
        searchtokens = tokenize(searchtokens[0]);
        mrcas[mrcaname] = searchtokens;
    }
    inmrca.close();
    
    string retstring;
    int ft = test_tree_filetype_stream(*pios, retstring);
    if (ft != 0 && ft != 1) {
        cerr << "this really only works with nexus or newick" << endl;
        exit(0);
    }
    
    bool going = true;
    map<string,vector<string> >::iterator it;
    
    if (ft == 1) {
        Tree * tree;
        while (going) {
            tree = read_next_tree_from_stream_newick(*pios, retstring, &going);
            if (tree != NULL) {
                for (it=mrcas.begin(); it != mrcas.end(); it++) {
                    Node * nd = tree->getMRCA((*it).second);
                    (*poos) << (*it).first << " " << nd->get_num_leaves() << " "
                        << nd->getName() << endl;
                }
                delete tree;
            }
        }
    } else if (ft == 0) { // Nexus. need to worry about possible translation tables
        map <string, string> translation_table;
        bool ttexists;
        ttexists = get_nexus_translation_table(*pios, &translation_table, &retstring);
        Tree * tree;
        while (going) {
            tree = read_next_tree_from_stream_nexus(*pios, retstring, ttexists,
                &translation_table, &going);
            if (tree != NULL) {
                for (it=mrcas.begin(); it != mrcas.end(); it++) {
                    Node * nd = tree->getMRCA((*it).second);
                    (*poos) << (*it).first << " " << nd->get_num_leaves() << " "
                        << nd->getName() << endl;
                }
                delete tree;
            }
        }
    }
    
    if (fileset) {
        fstr->close();
        delete pios;
    }
    if (outfileset) {
        ofstr->close();
        delete poos;
    }
    
    return EXIT_SUCCESS;
}
