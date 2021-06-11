#include <bits/stdc++.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <grp.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;
//=========================NAME: SHIV SHARMA ===================================================//
//======================ROLL NO: 2020201037=====================================================//



//=======================global variables=======================================================//

//original terminal attributes
struct termios orig_termios;

//cursor pointer
int current_row = 0;
int current_col = 0;

//terminal size 
int rows = 0;
int col=0;

//forward and backward stack
vector<string> forward_stack, backward_stack;
vector<string> files;

// current terminal range of files
int lowerrange=0, highrange=rows-1;

//application root
string root;


// ================ For getting back in non canonical mode=======================================//
void disableRawMode()
{
    printf("\033[H\033[J");
    printf("\033[1;1H");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// ================ For getting into canonical mode==============================================//
void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    // printf("%c[%d;%dH",27,25,25);
}

// ================ For printing file information on the terminal with file path as argument=================//
void display(string filepath)
{
    struct stat info;
    
    if(stat(filepath.c_str(), &info) == -1){
        printf("error");
    }
    
    // printf("test %s    ",cur_file->d_name);
    //permissions
    printf((S_ISDIR(info.st_mode)) ? "d" : "-");
    printf((info.st_mode & S_IRUSR) ? "r" : "-");
    printf((info.st_mode & S_IWUSR) ? "w" : "-");
    printf((info.st_mode & S_IXUSR) ? "x" : "-");
    printf((info.st_mode & S_IRGRP) ? "r" : "-");
    printf((info.st_mode & S_IWGRP) ? "w" : "-");
    printf((info.st_mode & S_IXGRP) ? "x" : "-");
    printf((info.st_mode & S_IROTH) ? "r" : "-");
    printf((info.st_mode & S_IWOTH) ? "w" : "-");
    printf((info.st_mode & S_IXOTH) ? "x" : "-");

    //user and group
    auto user = getpwuid(info.st_uid);
    printf("\t%s", user->pw_name);
    auto group = getgrgid(info.st_gid);
    printf("\t%s", group->gr_name);

    //file size
    printf("\t%lld", (long long int)info.st_size);

    //modification time
    char mtime[100];
    strftime(mtime, sizeof(mtime), "%a %Y-%m-%d %H:%M:%S", localtime(&(info.st_mtime)));
    printf("\t%s", mtime);

    //file name
     char *cstr = &filepath[0];
    printf("\t\t%s\n",(basename(cstr)) );
}

// ================ For command mode file creation ==============================================//
string createfile(string name ,string destination){
    destination = destination + "/" + name;
    FILE *f = fopen(destination.c_str(),"w+");
    if(f) return("file created");
    else return "error in file creation";
}

//================= For command mode directory creation =========================================//
string createdir(string director_name, string destination){
    
    //here directory permission are already predefined (0755) and the owner is the with which user id the program is run
    if(strcmp(director_name.c_str(),"") != 0)destination = destination + "/" + director_name;
    
    if(mkdir(destination.c_str(),0755)) return("Error in directory creation!");
    else return "Directory created";
    return "";
}

//================= For command mode file copy ==================================================//
string copyfile(string sourcefile, string destinationfile){
    //the file permission are set using the stat and send system call is used for file copy
    int source ;
    int dest ;

    if((source = open(sourcefile.c_str(), O_RDONLY, 0) ) == -1) return "Error reading the given source file";
    if((dest = open(destinationfile.c_str(), O_WRONLY | O_CREAT , 0644)) == -1) return "Error writing the given destination file";

    
    struct stat stat_source;
    if(fstat(source, &stat_source) != 0)return "Error reading the given source file";

    if(sendfile(dest, source, 0, stat_source.st_size) == -1) return "error copying";

    close(source);
    close(dest);
    
    if(chmod(destinationfile.c_str(),stat_source.st_mode) != 0) return "error changing permission";
    if(chown(destinationfile.c_str(),stat_source.st_uid,stat_source.st_gid) !=0) return "error changing owner";
    return "Copied!!";


}

//================= For command mode directory copy =============================================//
string copydir(string tobecopied, string newfolderpath){
    DIR *root_dir = opendir(tobecopied.c_str());
    struct dirent *cur_file;

    if (root_dir)
    {

        while ((cur_file = readdir(root_dir)))
        {
            
            //checking for "." and ".." directory first
            if( (strcmp(cur_file->d_name,"..") == 0) ||(strcmp(cur_file->d_name,".") == 0 ) ){
                continue;
            }

            else{

                string filename(cur_file->d_name);
                string file_to_be_copied = tobecopied + "/" + filename;
                string copied_file = newfolderpath + "/" + filename;
                
                struct stat info;
                stat(file_to_be_copied.c_str(),&info);

                //if a regular file call copy file 
                if(!S_ISDIR(info.st_mode)){
                      string exitstatus = copyfile(file_to_be_copied,copied_file);
                      if( exitstatus != "Copied!!")
                      return exitstatus;
                }
                
                // if a directory call copy directory recursively
                else{
                    mkdir(copied_file.c_str(),info.st_mode);
                    string exit_status = copydir(file_to_be_copied,copied_file);
                    if(chown(copied_file.c_str(),info.st_uid,info.st_gid) !=0) return "error changing owner";

                    if(exit_status != "Copied") return exit_status;
                }

            }
        }
        
        closedir(root_dir);
        return "Copied";  
    }
    else return "No such directory!!";
}

//================= For command mode search operation ===========================================//
string search(string fname, string dir){
    DIR *root_dir = opendir(dir.c_str());
    struct dirent *cur_file;

    if (root_dir)
    {

        while ((cur_file = readdir(root_dir)))
        {
            if( 
                (strcmp(cur_file->d_name,"..") == 0) ||(strcmp(cur_file->d_name,".") == 0 ) 
            ){
                continue;
            }

            else{

                string filename(cur_file->d_name);
                if(filename == fname) return ("Found at" + dir);
                string newfile = dir + "/" + filename;
                struct stat info;
                stat(newfile.c_str(),&info);

                if(S_ISDIR(info.st_mode)){
               
                    string exit_status = search(fname,newfile);
                    if(exit_status.substr(0,8) == "Found at") return exit_status;
                }

            }
        
        }
        
        closedir(root_dir);
        return "Not found";  
    }
    else return "No such directory!!";
}

//================= For command mode file delete ================================================//
string deletefile(string filepath){
    if(remove(filepath.c_str()) == 0) return "file deleted";
    else return "error in deleting file";
}

//================= For command mode directory delete ===========================================//
string deletedir(string deletepath){
    DIR *root_dir = opendir(deletepath.c_str());
    struct dirent *cur_file;

    if (root_dir)
    {

        while ((cur_file = readdir(root_dir)))
        {
            if( 
                (strcmp(cur_file->d_name,"..") == 0) ||(strcmp(cur_file->d_name,".") == 0 ) 
            ){
                continue;
            }

            else{

                string filename(cur_file->d_name);
                string filedeletepath = deletepath + "/" + filename;
                struct stat info;
                stat(filedeletepath.c_str(),&info);

                if(!S_ISDIR(info.st_mode)){
                      string exitstatus = deletefile(filedeletepath);
                      if( exitstatus != "file deleted")
                      return exitstatus;
                }
                else{
                    
                    string exit_status = deletedir(filedeletepath);
                    if(exit_status != "directory deleted") return exit_status;
                    
                }





            }
        
        
        
        
        }
        
        closedir(root_dir);
        if(remove(deletepath.c_str()) != 0) return "Error deleting directory";
        return "directory deleted";  
    }
    else return "No such directory!!";
}

//================= For printing range between lowerrange and upperrange ========================//
void print_range(vector<string> &files, int lowerrange,int highrange){
    printf("\033[H\033[J");
    printf("\033[1;1H");
    
    for (int i = lowerrange; i < (int)files.size() && i < highrange; i++)
        {
            if(i < 0) i++;
            else display(files[i]);
        }
    current_row = 1 , current_col = 1;
    printf("%c[%d;%dH", 27, current_row, current_col);
}

//================= Defination of normal mode ===================================================//
void normalmode(string cwd_string);

//================= Helper function for priting status at the status bar ========================//
void print_status(string st){
    current_row = rows, current_col = 1;
    printf("%c[%d;%dH", 27, current_row, current_col);
    printf("%c[2K\r", 27);
    printf(":%s ",st.c_str());
    current_row = 1, current_col = 1;
    printf("%c[%d;%dH", 27, current_row, current_col);
}

//================== Storing the files path in the files vector =================================//
string opendirectory(string directory)
{
    //clear and set the cursor to the top left
    printf("\033[H\033[J");
    printf("\033[1;1H");

    lowerrange=0, highrange=rows-1;
    files.clear();
    current_row = 1, current_col = 1;
    DIR *root_dir = opendir(directory.c_str());
    struct dirent *cur_file;

    if (root_dir)
    {

        while ((cur_file = readdir(root_dir)))
        {
            files.push_back(directory+"/"+ cur_file->d_name);
        }
        closedir(root_dir);   
    }
    
    sort(files.begin(),files.end());
    return ("Opened " + directory);

}

//================= Main command mode function =================================================//
string commandmode(string directory){
    
    string s; char c;

    // implementing exit functionality with ESC and command enter with <Enter>
    while ((c = cin.get()) != 10){
        if(c == 27) return("command mode exit");
        else if(c == 127){
            if(current_col > 1){s.pop_back();
            printf("%c[%d;%dH", 27, current_row, current_col);
            printf(" ");
            printf("%c[%d;%dH", 27, current_row, current_col);
            current_col--;

            }
        }
        else{
            printf("%c",c);
            s.push_back(c);
            current_col++;

        }
    }


    //commands vector for storing command name and its parametes
    vector<string> commands;
    stringstream ss(s);
    string parameters;
    while(ss >> parameters){
        commands.push_back(parameters);
    }

    // now checking different commands
    if(commands[0] == "rename"){
        if(commands.size() == 3){
            
            //adding path to file names
            commands[1] = directory+"/"+commands[1];
            commands[2] = directory+"/"+commands[2];

            int exit_status = rename(commands[1].c_str(),commands[2].c_str());

            if(exit_status == 0){
                return("File renamed");
                
            }
            else return("File not found");

        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "create_file"){
        
        if(commands.size()  == 3){
            //formatting the destination
            if(commands[2][0] == '~') commands[2] = root +commands[2].substr(1);
            else commands[2] = directory +"/"+ commands[2];
            return createfile(commands[1],commands[2]);
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "create_dir"){
        if(commands.size()  == 3){
            // formatting the destination
            if(commands[2][0] == '~') commands[2] = root +commands[2].substr(1);
            else commands[2] = directory +"/"+ commands[2];
            return createdir(commands[1],commands[2]);
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "copy"){
        if(commands.size()  >= 3){
            
            //formatting path
            //if home    
            if(commands[commands.size()-1][0] == '~') 
                commands[commands.size()-1] = root +commands[commands.size()-1].substr(1);
            //else directory given
            else commands[commands.size()-1] = directory +"/"+ commands[commands.size()-1];

            for(int i = 1 ; i < (int)commands.size()-1; i++){
            
            string filepath = directory + "/" + commands[i];
            struct stat info;
            stat(filepath.c_str(),&info);

            // check if file
            if(!S_ISDIR(info.st_mode)){
                string newfile_path = commands[commands.size()-1] + "/" + commands[i];
                string exit_status =  copyfile(filepath,newfile_path);
                if(exit_status != "Copied!!") return exit_status;
            }
            else{
                createdir(commands[i],commands[commands.size()-1]);
                string newdir_path = commands[commands.size()-1] + "/" + commands[i];
                string exit_status =  copydir(filepath,newdir_path);
                if(exit_status != "Copied!!") return exit_status;
            }


            }
            return "Copied";    

            
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "delete_file"){
        if(commands.size()  == 2){
            if(commands[1][0] == '~') commands[1] = root +commands[1].substr(1);
            else commands[1] = directory +"/"+ commands[1];
            return deletefile(commands[1]);
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "delete_dir"){
        if(commands.size()  == 2){
            if(commands[1][0] == '~') commands[1] = root +commands[1].substr(1);
            else commands[1] = directory +"/"+ commands[1];
            return deletedir(commands[1]);
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "move"){
        if(commands.size()  >= 3){
            
            //formatting path
            //if home    
            if(commands[commands.size()-1][0] == '~') 
                commands[commands.size()-1] = root +commands[commands.size()-1].substr(1);
            //else directory given
            else commands[commands.size()-1] = directory +"/"+ commands[commands.size()-1];

            for(int i = 1 ; i < (int)commands.size()-1; i++){
            
            string filepath = directory + "/" + commands[i];
            struct stat info;
            stat(filepath.c_str(),&info);

            // check if file
            if(!S_ISDIR(info.st_mode)){
                string newfile_path = commands[commands.size()-1] + "/" + commands[i];
                string exit_status_copied =  copyfile(filepath,newfile_path);
                string exit_status_delete = deletefile(filepath);
                if(exit_status_copied != "Copied!!") return exit_status_copied;
                if( exit_status_delete != "file deleted") return exit_status_delete;
            }
            else{
                createdir(commands[i],commands[commands.size()-1]);
                string newdir_path = commands[commands.size()-1] + "/" + commands[i];
                string exit_status_copied =  copydir(filepath,newdir_path);
                string exit_status_delete = deletedir(filepath);
                if(exit_status_copied != "Copied") return exit_status_copied;
                if (exit_status_delete != "directory deleted") return exit_status_delete;

            }


            }
            return "moved";    

            
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "goto"){
        if(commands.size()  == 2){
            if(commands[1][0] == '~') commands[1] = root +commands[1].substr(1);
            else commands[1] = root +"/"+ commands[1];
            struct stat info;
            if(stat(commands[1].c_str(),&info) != 0 ) return "error opening path";
            if(  S_ISDIR(info.st_mode) ||  S_ISLNK(info.st_mode)){
                backward_stack.push_back(directory);
                forward_stack.clear();
                string status = opendirectory(commands[1].c_str());

                
                lowerrange=0, highrange=rows-1;
                
                print_range(files,lowerrange,highrange);
                print_status(status);


            }
            else return "Not a directory";

            return ("Opened directory"+commands[1]);
        }
        else return("Invalid parameters");
    }
    
    else if(commands[0] == "search"){
        if(commands.size()  == 2){
            
            return search(commands[1],root);
        }
        else return("Invalid parameters");
    }
    
    else{
        return "No such command";
    }

}

//================= Main normal mode function ==================================================//
void normalmode(string cwd_string){
    
    
    //non canon mode
    enableRawMode();
    
    root = cwd_string;
    //opening home
    string status = opendirectory(cwd_string);
    
    lowerrange=0, highrange=rows-1;
    print_range(files,lowerrange,highrange);
    print_status(status);

    string directory = cwd_string;

    //handling inputs
    char c[3];
    while ((c[0] = cin.get()) != 'q')
    {
        //for up down left right arrow implementation
        if (c[0] == 27 )
        {
            
            if((c[1] = cin.get()) == 91){
            c[2] = cin.get();

            //down
            if (c[2] == 'B' && (current_row < rows-1) && current_row < files.size())
            {
                printf("%c[%d;%dH", 27, ++current_row, current_col);
            }

            //up
            if (c[2] == 'A' && current_row > 1)
            {
                printf("%c[%d;%dH", 27, --current_row, current_col);
            }

            //back
            if (c[2] == 'D' && backward_stack.size() > 0)
            {
                string back_dir = backward_stack.back();
                backward_stack.pop_back();
                forward_stack.push_back(directory);
                directory = back_dir;
                status  = opendirectory(directory);
                
                print_range(files,lowerrange,highrange);
                print_status(status);

            }
            
            //forward
            if (c[2] == 'C' && forward_stack.size() > 0)
            {
                string forward_dir = forward_stack.back();
                forward_stack.pop_back();
                backward_stack.push_back(directory);
                directory = forward_dir;
                status  = opendirectory(directory);

                print_range(files,lowerrange,highrange);
                print_status(status);


            }
        }}
        
        //enter
        if (c[0] == 10 ){
            struct stat info;

            if(stat(files[lowerrange+ current_row-1].c_str(), &info) == -1){
                printf("Error");
            }
            
            //if regular file
            if (S_ISREG(info.st_mode)){
                int pid = fork();
                    if (pid == 0) {
                        close(2);
                        execlp("/usr/bin/xdg-open", "xdg-open", files[lowerrange+ current_row-1].c_str(), NULL);
                        exit(1);
                    }
                // exit(0);
            }
            
            //if directory or symbolic link
            if(  S_ISDIR(info.st_mode) ||  S_ISLNK(info.st_mode)){
                backward_stack.push_back(directory);
                forward_stack.clear();
                status = opendirectory(files[lowerrange+current_row-1].c_str());

                directory = files[lowerrange+current_row-1];
                lowerrange=0, highrange=rows-1;
                
                print_range(files,lowerrange,highrange);
                print_status(status);


            }
        }
        
        //backspace
        if(c[0] == 127){

            
            backward_stack.push_back(directory);
            forward_stack.clear();
            status = opendirectory(directory+"/..");
            directory = directory+"/..";
            print_range(files,lowerrange,highrange);
            print_status(status);
        }
        
        //for h home
        if(c[0] == 'h' && backward_stack.size() > 0){
            backward_stack.push_back(directory);
            forward_stack.clear();
            status = opendirectory(root);
            directory = backward_stack[0];
            print_range(files,lowerrange,highrange);
            print_status(status);

        }
        
        //for  going into command mode
        if(c[0] == 58){
            current_row = rows, current_col = 1;
            printf("%c[%d;%dH", 27, current_row, current_col);
            printf("%c[2K\r", 27);
            printf(":");
            string es = commandmode(directory);
            stringstream ss(es);

            if(es.substr(0,16) == "Opened directory"){
                directory = es.substr(16);
                continue;
            }
            opendirectory(directory);
            lowerrange=0, highrange=rows-1;
            print_range(files,lowerrange,highrange);
            print_status(es);

        }
        
        //for scrolling implementation downwards
        if(c[0] == 'k'){
            //setting range
            if(highrange < files.size()){lowerrange++; 
            highrange++;}
            //print values
            print_range(files,lowerrange,highrange);
            //cursor movement
            current_row =min(rows-1,(int)files.size());
            printf("%c[%d;%dH", 27, current_row, 1);

        }
        
        //for scrolling implementation upwards
        if(c[0] == 'l'){
            //setting range
            if(lowerrange > 0){lowerrange--; 
            highrange--;}
            //print values
            print_range(files,lowerrange,highrange);
            //cursor movement
            printf("%c[%d;%dH", 27, 1, 1);
            current_row = 1;

        }
    }
    
    // exit with "q"
    if(c[0] == 'q') exit(0);
    
}

int main()
{
    //terminal size
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    rows = w.ws_row, col = w.ws_col;
    

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    string cwd_string(cwd);
    
    // starting normal mode with appication root as parameter
    normalmode(cwd_string);
    

    return 0;
}