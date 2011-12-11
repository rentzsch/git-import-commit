# git-import-commit

**git-import-commit** is a small command-line tool that imports a source repo's commit (and all its associated trees and blobs) into a destination repo as a "new" commit (it updates the given branch's head).

Unlike git-cherry-pick, the commit itself is not rewritten in any way (it's copied wholesale) and the imported commit's tree can be *entirely* different than what's currently in the destination repo. The imported commit will have a different SHA, however, since its parent ID is different than it was in the source repo.

### Purpose

I wrote this tool after I migrated a ten-year-old CVS repo to git for client and then logged a few hundred commits before I realized I botched the initial migration (I used multiple `cvs2git ... module` invocations when I should have used a single `cvs2git ... cvsroot` on the entire repo (the dir with the `CVSROOT` dir in it) and lost most of the tags).

I was able to redo the initial migration and restore the tags (thanks backups+paranoia), but I still had those 200+ commits from the "future". I couldn't just merge them in since none of the parent commit IDs matched.

So I did something like this:

	$ cd bad-repo
	$ git rev-list HEAD|sed '1!G;h;$!d'|bbedit #sed magic to reverse lines
	(...edit out commit IDs from original migrated repo...)

That yielded a reverse-chronological listing of commits IDs to be imported into the newly migrated repo. I turned it into a dumb `.sh` script full of git-import-commit invocations that I ran:

	git-import-commit project-migration2/.git refs/heads/develop project-botched-base/.git 31a7fd5117d11fc4db6f19e8e52219cee5a7ec87 --quiet
	git-import-commit project-migration2/.git refs/heads/develop project-botched-base/.git e321b08b83cd194a477951f95b84d7ed52fbaf68 --quiet
	git-import-commit project-migration2/.git refs/heads/develop project-botched-base/.git 50b8f1939e1ae1c49fc281abaf90cdbf7efc253b --quiet
	git-import-commit project-migration2/.git refs/heads/develop project-botched-base/.git 8a89f790cd2049766ddbae0696af01c45703cfe8 --quiet
	...

When run in default verbose mode, git-import-commit prints the commit tree. It prefixes each dir and file with either an equal sign (=) or a plus (+). The equal sign means the dir/tree or file/blob was already found in the destination repo and didn't need to be copied over, while the plus indicates a copy was needed.

All that said, you probably won't ever need this tool unless you're up against a similar wall.

### Dependancies

git-import-commit relies on libgit2, which is included as a fake submodule.

### FIXME

There's a bunch of limitations that I didn't bother to deal with since they didn't bite me:

* Tags aren't imported.

* Commits with multiple parents are flattened down to one parent, losing information.

* It would be cool to stream blobs when copying instead of buffering their entirety into memory.

* Argument handling is pathetic (should use getopt or some such).

* While the code should be pretty much cross-platform, only an .xcodeproj file is provided.