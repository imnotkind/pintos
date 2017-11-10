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

#or you could do...
make tests/threads/alarm-multiple.result

#reference solution's fixed code files:
PROJECT 1
devices/timer.c | 42 +++++-
threads/fixed-point.h | 120 ++++++++++++++++++
threads/synch.c | 88 ++++++++++++-
threads/thread.c | 196 ++++++++++++++++++++++++++----
threads/thread.h | 23 +++
fixed-point.h is a new file added.


PROJECT 2
threads/thread.c | 13
threads/thread.h | 26 +
userprog/exception.c | 8
userprog/process.c | 247 ++++++++++++++--
userprog/syscall.c | 468 ++++++++++++++++++++++++++++++-
userprog/syscall.h | 1
6 files changed, 725 insertions(+), 38 deletions(-)

PROJECT 3
Makefile.build | 4
devices/timer.c | 42 ++
threads/init.c | 5
threads/interrupt.c | 2
threads/thread.c | 31 +
threads/thread.h | 37 +-
userprog/exception.c | 12
userprog/pagedir.c | 10
userprog/process.c | 319 +++++++++++++-----
userprog/syscall.c | 545 ++++++++++++++++++++++++++++++-
userprog/syscall.h | 1
vm/frame.c | 162 +++++++++
vm/frame.h | 23 +
vm/page.c | 297 ++++++++++++++++
vm/page.h | 50 ++
vm/swap.c | 85 ++++
vm/swap.h | 11
17 files changed, 1532 insertions(+), 104 deletions(-)

https://github.com/codyjack/OS-pintos
