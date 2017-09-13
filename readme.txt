이 프로젝트는 포스텍 OS 수업의 PintOS 프로젝트를 위한 것입니다.

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




#ctags help

make tags file by 
    ctags -R (recursive) 
    ctags *

vim tags #if you don't want to open tags, set ~/.vimrc
:tj func # go to func (define or function name) #Ctrl+]
:po  # go backwards                             #Ctrl+t



