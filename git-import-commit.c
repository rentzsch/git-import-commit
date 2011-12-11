// git-import-commit.c 1.0
//   Copyright (c) 2011 Jonathan 'Wolf' Rentzsch: http://rentzsch.com
//   Some rights reserved: http://opensource.org/licenses/MIT
//   https://github.com/rentzsch/git-import-commit

#include <git2.h>

int conditionallyCopyTree(git_repository *srcRepo,    // -> 
                          git_tree *srcTree,          // -> 
                          git_repository *dstRepo,    // -> 
                          git_tree **dstTree,         // <-
                          const char *treeEntryName,  // -> 
                          unsigned indentCount);      // -> 

int recursivelyConditionallyCopyTree(git_repository *srcRepo,
                                     git_tree *srcTree,
                                     git_repository *dstRepo,
                                     unsigned indentCount);

int gVerbose = 1;

int main(int argc, const char *argv[]) {
    //
    // Process args.
    //
    if (!(argc == 5 || argc == 6)) {
        fprintf(stderr, "Usage: %s path/to/dst/.git refs/heads/master path/to/src/.git d76a55fa [--quiet]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char *dstRepoPath       = argv[1];
    const char *dstCommitRefName  = argv[2];
    const char *srcRepoPath       = argv[3];
    const char *srcCommitID       = argv[4];
    
    if (argc == 6) {
        gVerbose = 0;
    }
    
    int err = GIT_SUCCESS;
    
    //
    // Open repos.
    //
    
    git_repository *srcRepoObj = NULL;
    if (!err) {
        err = git_repository_open(&srcRepoObj, srcRepoPath);
    }
    
    git_repository *dstRepoObj = NULL;
    if (!err) {
        err = git_repository_open(&dstRepoObj, dstRepoPath); 
    }
    
    //
    // Find destination repo's last commit, which we'll use as the new commit's parent.
    //
    
    git_reference *dstCommitRef = NULL;
    if (!err) {
        err = git_reference_lookup(&dstCommitRef, dstRepoObj, dstCommitRefName);
    }
    git_reference *dstResolvedCommitRef = NULL;
    if (!err) {
        err = git_reference_resolve(&dstResolvedCommitRef, dstCommitRef);
    }
    const git_oid *dstCommitOID;
    if (!err) {
        dstCommitOID = git_reference_oid(dstResolvedCommitRef);
        char dstHeadStr[GIT_OID_HEXSZ+1];
        git_oid_fmt(dstHeadStr, dstCommitOID);
        dstHeadStr[GIT_OID_HEXSZ] = 0;
        if (gVerbose) {
            printf("\nusing destination ref %s (%s) for parent commit\n", dstCommitRefName, dstHeadStr);
        }
    }
    git_commit *dstCommitObj = NULL;
    if (!err) {
        err = git_commit_lookup(&dstCommitObj, dstRepoObj, dstCommitOID);
    }
    
    //
    // Find source repo's commit.
    //
    
    git_oid srcCommitOID;
    if (!err) {
        err = git_oid_fromstr(&srcCommitOID, srcCommitID);
    }
    git_commit *srcCommitObj = NULL;
    if (!err) {
        err = git_commit_lookup_prefix(&srcCommitObj, srcRepoObj, &srcCommitOID, strlen(srcCommitID));
    }
    git_tree *srcCommitTreeObj = NULL;
    if (!err) {
        if (gVerbose) {
            const char *encoding = git_commit_message_encoding(srcCommitObj);
            printf("encoding: %s\n", encoding ? encoding : "NULL (UTF-8 assumed)");
            printf("message: %s\n", git_commit_message(srcCommitObj));
            printf("time: %lld\n", git_commit_time(srcCommitObj));
            printf("offset: %d\n", git_commit_time_offset(srcCommitObj));
            const git_signature *sig = git_commit_committer(srcCommitObj);
            printf("committer: %s %s %lld %d\n", sig->name, sig->email, sig->when.time, sig->when.offset);
            sig = git_commit_author(srcCommitObj);
            printf("author: %s %s %lld %d\n", sig->name, sig->email, sig->when.time, sig->when.offset);
            printf("parent count: %d\n", git_commit_parentcount(srcCommitObj));
        }
        
        err = git_commit_tree(&srcCommitTreeObj, srcCommitObj);
    }
    
    git_tree *dstCommitTreeObj = NULL;
    if (!err) {
        err = conditionallyCopyTree(srcRepoObj,
                                    srcCommitTreeObj,
                                    dstRepoObj,
                                    &dstCommitTreeObj,
                                    "<commit tree>",
                                    0);
    }
    if (!err) {
        err = recursivelyConditionallyCopyTree(srcRepoObj, srcCommitTreeObj, dstRepoObj, 1);
    }
    
    if (!err) {
        git_oid commitOID;
        const git_commit *parents[1] = { dstCommitObj };
        err = git_commit_create(&commitOID,
                                dstRepoObj,
                                dstCommitRefName,
                                git_commit_author(srcCommitObj),
                                git_commit_committer(srcCommitObj),
                                git_commit_message_encoding(srcCommitObj),
                                git_commit_message(srcCommitObj),
                                dstCommitTreeObj,
                                1,
                                parents);
    }
    
    //
    // Clean up.
    //
    
    if (dstResolvedCommitRef) {
        git_reference_free(dstResolvedCommitRef);
        dstResolvedCommitRef = NULL;
    }
    if (dstRepoObj) {
        git_repository_free(dstRepoObj);
        dstRepoObj = NULL;
    }
    if (srcRepoObj) {
        git_repository_free(srcRepoObj);
        srcRepoObj = NULL;
    }
    
    //
    // Report errors.
    //
    
    if (err) {
        printf("err: %d\n", err);
    }
    
    return 0;
}



void indentPrintf(unsigned indentCount) {
    indentCount *= 4;
    while (indentCount--) {
        printf(" ");
    }
}

int conditionallyCopyTree(git_repository *srcRepo,    // -> 
                          git_tree *srcTree,          // -> 
                          git_repository *dstRepo,    // -> 
                          git_tree **dstTree,         // <-
                          const char *treeEntryName,  // -> 
                          unsigned indentCount)       // -> 
{
    int err = GIT_SUCCESS;
    
    int shouldCopy = 0;
    if (!err) {
        git_object *dstTreeObj = NULL;
        err = git_object_lookup(&dstTreeObj, dstRepo, git_tree_id(srcTree), GIT_OBJ_TREE);
        
        if (GIT_ENOTFOUND == err) {
            err = GIT_SUCCESS;
            shouldCopy = 1;
        }
    }
    
    if (gVerbose) {
        printf(shouldCopy ? "+" : "=");
        indentPrintf(indentCount);
        printf("%s:\n", treeEntryName);
    }
    
    if (shouldCopy) {
        git_treebuilder *treeBuilder = NULL;
        if (!err) {
            err = git_treebuilder_create(&treeBuilder, srcTree);
        }
        
        git_oid treeOID;
        if (!err) {
            err = git_treebuilder_write(&treeOID, dstRepo, treeBuilder);
        }
        
        if (treeBuilder) {
            git_treebuilder_free(treeBuilder);
        }
    }
    
    if (!err && dstTree) {
        err = git_tree_lookup(dstTree, dstRepo, git_tree_id(srcTree));
    }
    
    return err;
}

int recursivelyConditionallyCopyTree(git_repository *srcRepo,
                                     git_tree *srcTree,
                                     git_repository *dstRepo,
                                     unsigned indentCount)
{
    int err = GIT_SUCCESS;
    
    unsigned entryCount = git_tree_entrycount(srcTree);
    for (unsigned entryIdx = 0; !err && entryIdx < entryCount; entryIdx++) {
        const git_tree_entry *entry = git_tree_entry_byindex(srcTree, entryIdx);
        const char *entryName = git_tree_entry_name(entry);
        git_otype entryObjectType = git_tree_entry_type(entry);
        
        git_object *srcEntryObj = NULL;
        err = git_tree_entry_2object(&srcEntryObj, srcRepo, entry);
        
        if (!err) {
            switch (entryObjectType) {
                case GIT_OBJ_TREE: {
                    git_tree *srcSubtree = (git_tree*)srcEntryObj;
                    err = conditionallyCopyTree(srcRepo,
                                                srcSubtree,
                                                dstRepo,
                                                NULL,
                                                entryName,
                                                indentCount);
                    if (!err) {
                        err = recursivelyConditionallyCopyTree(srcRepo, srcSubtree, dstRepo, indentCount + 1);
                    }
                }   break;
                case GIT_OBJ_BLOB: {
                    git_blob *dstBlobObj = NULL;
                    err = git_blob_lookup(&dstBlobObj, dstRepo, git_object_id(srcEntryObj));
                    
                    int shouldCopy = 0;
                    if (GIT_ENOTFOUND == err) {
                        err = GIT_SUCCESS;
                        shouldCopy = 1;
                    }
                    
                    if (gVerbose) {
                        printf(shouldCopy ? "+" : "=");
                        indentPrintf(indentCount);
                        printf("%s\n", entryName);
                    }
                    
                    if (!err && shouldCopy) {
                        git_blob *srcBlob = (git_blob*)srcEntryObj;
                        git_oid dstBlobOID;
                        err = git_blob_create_frombuffer(&dstBlobOID,
                                                         dstRepo,
                                                         git_blob_rawcontent(srcBlob),
                                                         git_blob_rawsize(srcBlob));
                    }
                }   break;
                default:
                    fprintf(stderr, "unexpected git_otype: %d\n", entryObjectType);
                    assert(0);
                    break;
            }
        }
    }
    
    return err;
}
