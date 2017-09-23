It's PINTOS TIME

####very kind guide by imnotkind

#if you want to add a file :
git add <file>
#if you want to remove a file :
git rm <file>

##after you made changes :
#make sure to use git add <file> that you have newly added
git commit -a -m "message about change" (you can add -a if you didn't rm your deleted file)

#if you want to apply your change to server :
git push origin master

#if you want to make sure that the source is identical with the server or roll back :
git fetch --all
git reset --hard origin/master
git pull origin master
#ONLY SOURCE is in git, so don't forget to make clean, build again!

#if you want to establish the source to a particular commit state
git reset --hard <hash>

#if you want to ignore some file or folder, go to .git/info/exclude and add the name of it.


#ctags help

make tags file by 
    ctags -R (recursive) 
    ctags *

vim tags #if you don't want to open tags, set ~/.vimrc
:tj func # go to func (define or function name) #Ctrl+]
:po  # go backwards                             #Ctrl+t


#run single test (see pass, fail) 
pintos -v -k -T 60 --qemu  -- -q  run alarm-single < /dev/null 2> tests/threads/alarm-single.errors > tests/threads/alarm-single.output
perl -I../.. ../../tests/threads/alarm-single.ck tests/threads/alarm-single tests/threads/alarm-single.result


pintos -v -k -T 60 --qemu  -- -q  run alarm-multiple < /dev/null 2> tests/threads/alarm-multiple.errors > tests/threads/alarm-multiple.output
perl -I../.. ../../tests/threads/alarm-multiple.ck tests/threads/alarm-multiple tests/threads/alarm-multiple.result


pintos -v -k -T 60 --qemu  -- -q  run alarm-simultaneous < /dev/null 2> tests/threads/alarm-simultaneous.errors > tests/threads/alarm-simultaneous.output
perl -I../.. ../../tests/threads/alarm-simultaneous.ck tests/threads/alarm-simultaneous tests/threads/alarm-simultaneous.result


pintos -v -k -T 60 --qemu  -- -q  run alarm-priority < /dev/null 2> tests/threads/alarm-priority.errors > tests/threads/alarm-priority.output
perl -I../.. ../../tests/threads/alarm-priority.ck tests/threads/alarm-priority tests/threads/alarm-priority.result

#or you could do...
make tests/threads/alarm-multiple.result

#reference solution's fixed code files:
devices/timer.c | 42 +++++-
threads/fixed-point.h | 120 ++++++++++++++++++
threads/synch.c | 88 ++++++++++++-
threads/thread.c | 196 ++++++++++++++++++++++++++----
threads/thread.h | 23 +++
fixed-point.h is a new file added.




