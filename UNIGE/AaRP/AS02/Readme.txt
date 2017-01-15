Assignment 02 - Advanced and Robot Programming
Ernest Skrzypczyk -- 4268738 // ES4268738-AaRP-AS02

#Files:
AS02-* -- Compiled binaries (on a x86_64 machine)
ES4268738-AaRP-AS02A-I.cpp -- Initializer source code
ES4268738-AaRP-AS02A-M.cpp -- Mediator source code
ES4268738-AaRP-AS02A-P1.cpp -- Publisher source code
ES4268738-AaRP-AS02A-P2.cpp -- A symlink to the first publisher
ES4268738-AaRP-AS02A-S1.cpp -- Subscriber source code
ES4268738-AaRP-AS02A-S2.cpp -- A symlink to the first subscriber
ES4268738-AaRP-AS02A-S3.cpp -- A symlink to the first subscriber
ES4268738-AaRP-AS02B.cpp -- Pthread version
ES4268738-AaRP-AS02.h -- Header files with configuration
ES4268738-AaRP-AS02b.h -- Header files with alternative configuration for the pthread version

#Scripts:
backup.sh -- used for backing up cpp files
filter.sh -- used for registering and filtering logs
read.sh -- read [filtered] logs
run.sh -- used to allow more control over execution
test.sh -- basic test scenario run script
update.sh -- recompile all binaries
extract.sh -- extract last archive
*-a -- pipe scripts version, can be run directly
*-b -- pthread script version, can be run directly

#Testing
To simply test the project, please run test.sh from the shell:
$ sh test.sh # or ./test.sh
since it is set with executable flag. There is one issue with running the scripts, namely if some of the subscribers are not satisfied, ergo if 
the are currently reading, they might hang and prevent the init process from exiting. Using pkill again might help here:
$ pkill -9 AS02A-
The program can be compiled with escape sequences defined in the headers, where also other parameters are defined, so that the output is more 
clear using color.

There are many switches in the source code so that using scripts or programs like sed, awk or grep the source code can be modified easily.

#Description
For your convenience the above scripts can be run to test the project more easily. Binaries can be also run directly with different parameters 
to change behaviour. Please notice that the code for publishers and subscribers is the same, so each subscriber has minimal differences in code 
that are the result of passed arguments to execl(). The reason behind is to make it possible to expand the structure using other subscribers 
and publishers. Please notice that there are many calculations in the code based on those parameters, so that this capability is provided. 
However because of time restraints certain aspects had to be implemented in a static way, which means there is code change necessary to 
actually fulfill the universal requirements. There might be still some misleading messages, because it is difficult to keep track of such a 
code base and much of the code was not optimized by implementing function calls.

Error handling has been implemented, so that all components can handle the SIGUSR1 signal send like this from the shell:
$ pkill -SIGUSR1 AS02A
or individually for one component
$ pkill -SIGUSR1 AS02A-P1
The return value for mediator was choosen to be 2, so that the code can be proven to work correctly.

Commented parts of the would be source code have been left for clarity sake. Certain parts of the code can be switched by 
commenting/uncommenting, so that different features can be tested or more information provided. For example there are semaphores synchronizing 
the first loop. This can be easily switched on and off by un/commenting the //~SEMAPHORE tagged lines. Semaphores serve only for this one purpose.

Instead of decaying message by message, it has been decided to flush the whole buffer should it overflow. This is triggered in two ways. First 
while writting a new message to the circular buffer, where number of free slots is determined and after the decay time, where the current time 
last initialization is being compared.

In the pthread version a vector, actually two vectors, one for the messages and one for the metadata, so that no further conversion is necessary, are
being used for the buffer[s]. Since it is a vector it automatically expands if the reserved or initial size is not sufficient for further writing 
operations. This makes the restrains put onto the assignment somewhat irrelevant. Therefore the decision has been made to simply limit the size to the 
given message buffer size, which has been fixed. However it can be also calculated based upon periods of components like shown in the pipe version. 
Here the header file has been modified, so that subscribers work faster, since the buffer size has been fixed. Of course the appropriate size could 
be calculated based upon periods of components, like already shown in the first version. However for the purpose of analizing the behaviour of the 
vector, the size has been fixed at value 8.

#Issues
The main issue was with alignment of variables that stored different kind of information. Char array are supposed to have as last element the 
\0 character. To make that happen I had to realign the circular buffer twice, which was painfully slow, since all pointers needed to be updated 
and a lot of debuggin was involved. Other big issue was with reading metadata. Since the decision based on the diagram you provided has been 
made to use one buffer for storing the content and metadata, both had to be of the same type, which is char in this case. This lead to issues 
with alignment, which was painfully slowly debugged and corrected.

A more aesthetic issue is that the display of the circular buffer. It is currently displayed each 10th loop. However usually when metadata gets 
updated and rewritten to the circular buffer, it does get displayed during those summaries. This due to the fact that the mediator actually handles
the requests so quickly.

As described in the testing part, it can happen that a subscriber gets blocked reading or any other component for that matter, when the signal 
handler is active. This might result in the initializer waiting forever for the process to finish. Usually rerunin the script helps. Also the 
commented code of using the pid of the process to modify waiting time could be used. 

In the pthread version there is the issue of size of the strings that are used to store messages. It so happens that they are 8 times as large 
as the message length, which is equal to MessageBuffer. However this was not further tested. There are some issues with end of string characters 
also and have also been skipped, because of time constraints. The proof of concept still holds however.

The main issue is that the vector does not properly delete messages with empty metadata. Even though it does occur correctly, a copy of the message 
remains under circumstances. It actually resides in the memory until overwritten and the size property of the vector changes. However updating the 
message counter to the appropriate size does not yield the expected results, which means either it is not done correctly or there is another issue 
involved.

Since there is a combination of usage of printf and cout, which occurs at different, even though sequential steps, the output gets distorted at times. 
It makes reading the log a little more difficult. This should be corrected using either printf or cout only, so that the command is atomatized, however 
the reason for using cout is the appropriate represantation of strings. The publishers also have a problem with that. Using additional string variables 
and adding the cout would help solve this, but introduce the mentioned problem of distortion.

#Snapshots
Since it took so much time to develop the code, I saved snapshots of the more recent version, which are in the included tars.
