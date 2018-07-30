#include "surfer.h"
extern string g_downPath;
string host_now;
const size_t np = string::npos;

//连接主机
int connectHost(const string &url){
    struct hostent *host;
    string hostUrl, pagePath;
    //将url解析为 主机名 和 网页路径
    parseHostAndPagePath(url, hostUrl, pagePath);    
    //通过主机名获取主机地址 失败则返回
    if(0==(host=gethostbyname(hostUrl.c_str()))){
        cout<<"gethostbyname error\n"<<endl;
        exit(-1);
    }
    host_now = hostUrl;

    //创建ip地址结构体 并将主机ip地址拷贝过去
    struct sockaddr_in pin;
    bzero(&pin,sizeof(pin));
    pin.sin_family = AF_INET;
    pin.sin_port = htons(80);
    pin.sin_addr.s_addr = ((struct in_addr*)(host->h_addr))->s_addr;
    //创建套接字
    int sfd;
    if((sfd = socket(AF_INET, SOCK_STREAM, 0))==-1){
        cout<<"socket open error\n"<<endl;
        return -1;
    }
    //连接主机
    if(connect(sfd, (const sockaddr*)&pin, sizeof(pin))==-1){
        cout<<"connect error\n"<<endl;
        return -1;
    }
    //返回套接字
    return sfd;
}

//获取网页
vector<char> getWebPage(int sfd,const string &url){
    struct hostent *host;
    string hostUrl, pagePath;
    //将url解析为 主机名 和 网页路径
    parseHostAndPagePath(url, hostUrl, pagePath);
    //对"www.baidu.com"将不会进行处理
    //hostUrl = "www.baidu.com"
    //pagePath = "/"    
    
    //创建https协议请求头 pagePath以/开头 hostUrl==www.baidu.com或baidu.com
    string requestHeader;
    requestHeader = "GET "+pagePath+" HTTP/1.1\r\n";
    requestHeader += "Host: "+hostUrl+"\r\n";    
    size_t pos = 0;
    if((pos=pagePath.find(".png")) != np)
        requestHeader += "Accept: image/png\r\n";
    else if((pos=pagePath.find(".jpg")) != np)
        requestHeader += "Accept: image/jpg\r\n";
    else if((pos=pagePath.find(".gif")) != np)
        requestHeader += "Accept: image/gif\r\n";
    else if((pos=pagePath.find(".shtml")) != np)
        requestHeader += "Accept: text/shtml\r\n";
    else
        requestHeader += "Accept: text/html\r\n";
    requestHeader += "User-Agent: Mozilla/5.0\r\n";
    requestHeader += "connection:Keep-Alive\r\n";
    requestHeader += "\r\n";
     
    //发送协议头
    if(send(sfd, requestHeader.c_str(), requestHeader.size(), 0)==-1){
        cout<<"requestHeader send error\n"<<endl;
        exit(1);
    }
    //设置socket选项
    struct timeval timeout={1,0};
    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(struct timeval));
    //接收应答消息
    int len, BUFFER_SIZE=1024;
    char buffer[BUFFER_SIZE];
    vector<char> vbytes;
    vector<char> vcontent;
    vector<char> vhttpHead;
    while((len = recv(sfd, buffer, BUFFER_SIZE-1, 0))>0){
        for(int i=0;i<len;i++)
            vbytes.push_back(buffer[i]);
    }
	//包含协议头的完整数据 写入本地文件
	//writeLocalFile(vbytes,url+"_full.html",g_downPath);

	//将http协议头和网页内容分离
    int i=0;
    if(vbytes.size()>=4){
    	for(i=4;i<vbytes.size();i++)
	    	if(vbytes[i-4]=='\r' && vbytes[i-3]=='\n' && vbytes[i-2]=='\r' && vbytes[i-1]=='\n')
		    	break;
    }else
        return vbytes;

    //得到http协议头 并根据解析结果分别处理
    vhttpHead.insert(vhttpHead.end(),vbytes.begin(),vbytes.begin()+i-1);
    string httpHead = vhttpHead.data();
    string general = httpHead.substr(0,httpHead.find("\r\n"));
    string server,date,contentType,contentLength,connection,location;

    size_t start = httpHead.find("Server:"), end = 0;
    if(start != np){
        end = httpHead.find("\r\n",start);
        server = httpHead.substr(start,end-start);
    }    
    start = httpHead.find("Date:");
    if(start != np){
        end = httpHead.find("\r\n",start);
        date = httpHead.substr(start,end-start);
    }
    start = httpHead.find("Content-Type:");
    if(start != np){
        end = httpHead.find("\r\n",start);
        contentType = httpHead.substr(start,end-start);
    }
    start = httpHead.find("Content-Length:");
    if(start != np){
        end = httpHead.find("\r\n",start);
        contentLength = httpHead.substr(start,end-start);
    }
    start = httpHead.find("Connection:");
    if(start != np){
        end = httpHead.find("\r\n",start);
        connection = httpHead.substr(start,end-start);
    }
    start = httpHead.find("Location:");
    if(start != np){
        end = httpHead.find("\r\n",start);
        location = httpHead.substr(start,end-start);
    }
    //如果网页返回错误代码 输出查看当前httpHead 并返回
    if(general.find("200") == np){
        cout << "\t" << "resource not found !" << endl;
        cout << "\t" << general << endl; //general是肯定存在的 后面的项目不一定存在
        cout << "\t" << server << (server.size()>0 ? "\n\t":"");
        cout << date << (date.size()>0 ? "\n\t":"");
        cout << contentType << (contentType.size()>0 ? "\n\t":"");
        cout << contentLength << (contentLength.size()>0 ? "\n\t":"");
        cout << connection << (connection.size()>0 ? "\n\t":"");
        cout << location << endl;
        //注意：三目表达式 涉及到类型不同的 或者前扩 或者全扩 编译器才能正确解析 否则会报错
        return vbytes;
    }

    //得到内容
	vcontent.insert(vcontent.end(),vbytes.begin()+i,vbytes.end());
	
    //如果是网页内容 就在本地生成网页文件 包括html或shtml
    if(requestHeader.find("html") != np){
        //去除<!doctype html>前面和后面可能存在的数字
        if(vcontent.size()>=10){
            for(i=0;i<10;i++)
                if(vcontent[i]=='<'){
                    vcontent.erase(vcontent.begin(),vcontent.begin()+i);
                    break;
                }
            int size = vcontent.size();
            for(i=0;i<10;i++)
                if(vcontent[size-1-i]=='>'){
                    for(int j=0;j<i;j++)
                        vcontent.pop_back();
                    break;
                }
        //极端情况下 vbytes.size()<10，就直接退出            
        }else
            return vbytes;       

        //将文件名中的/替换为.
        string pageName = hostUrl+pagePath;
        size_t b=0, p=0;
        if(pageName.find("/") != np)            
            while(true){
                b = pageName.find("/",p);
                if(b == np) break;
                pageName.replace(b,1,".");
                p = b+1;
            }            
        //cout << "pageName: " << pageName << endl;

        //将网页写入本地
        //请求.shtml时 pageName以.shtml结尾
        if(requestHeader.find("shtml") != np)
            writeLocalFile(vcontent,pageName,g_downPath);
        //找不到shtml 就是普通网页 以.html结尾  或 没有结尾
        else{
            if(pageName.find(".html") == np)
                writeLocalFile(vcontent,pageName+".html",g_downPath);
            else
                writeLocalFile(vcontent,pageName,g_downPath);
        }        
    }
	//如果不是html 就生成本地文件 文件名为
    else{
        //检查对应文件夹是否存在 若不存在 则创建之
        string downPath = g_downPath + "/" + host_now;
        if(access(downPath.c_str(),R_OK|W_OK|X_OK) == -1){
            if(mkdir(downPath.c_str(),0777) == -1){
                perror("mkdir error");
                return vbytes;
            }else
                printf("dir created OK:%s\n",downPath.c_str());
        }
        //获取有效的文件名 如果存在%或= 都去除之
        string filename = pagePath.substr(pagePath.rfind("/")+1);
        if(filename.find("%") != np)
            filename = filename.substr(filename.rfind("%")+1);
        if(filename.find("=") != np)
            filename = filename.substr(filename.rfind("=")+1);

        writeLocalFile(vcontent,filename,downPath,"\t");
    }        

    return vcontent;
}

//抽取网页资源
void drawResources(int sfd,const vector<char> &vcontent){
    //将网页内容写入本地文本
    // writeLocalFile(pageContent,"www.baidu.com_index.html",g_downPath);
    //获取页面内容中的https     
    list<string> url_list = getHttps(vcontent); 

    if(url_list.size() == 0){
        cout << "found no urls in \"" << host_now << "\"" << endl;
        return;
    }
    //将资源链接列表输出到本地txt文档`
    writeLocalFile(url_list,"url_list.txt",g_downPath);

    int cnt=0;
    for(auto url:url_list){
        cnt++;
        cout << "downloading file "<< cnt << ": "<< url << endl;
        getWebPage(sfd,url);
    }
    cout << endl << "download over." << endl;
}

//网址解析
void parseHostAndPagePath(const string& url, string &hostUrl, string &pagePath){
    hostUrl=url; //url完整副本
    pagePath="/";
    //查找协议前缀 如果找到了 将协议前缀置换为空
    size_t pos=hostUrl.find("http://");
    if(pos != np)
        hostUrl=hostUrl.replace(pos,7,"");
    pos=hostUrl.find("https://");
    if(pos != np)
        hostUrl=hostUrl.replace(pos,8,"");
    //找到主机url中的第一个层次划分
    pos=hostUrl.find("/");
    //如果存在层次划分 则分别重置页面路径和主机地址
    if(pos != np){
        pagePath=hostUrl.substr(pos);
        hostUrl=hostUrl.substr(0,pos);
    }
    //对"www.baidu.com"将不会进行处理
    //hostUrl = "www.baidu.com"
    //pagePath = "/"
}

//后续可以追加资源类型标签 分类获取不同类型的资源地址
list<string> getHttps(const vector<char> &vcontent,const char* type/*="images"*/){
    list<string> url_list;
    size_t begin=0,end=0,b1=0,e1=0;
    string url,pageContent;
    for(int i=0;i<vcontent.size();i++)
        pageContent.push_back(vcontent[i]);

    int cnt = 0;
    while(true){
        begin = pageContent.find("url(",begin);
        if(begin == np) break;
        end = pageContent.find(")",begin);
        url = pageContent.substr(begin+4,end-begin-4);
        //去除url字符串中的'\'符号
        url.erase(remove(url.begin(),url.end(),'\\'),url.end());
        //去除url字符串中的‘
        url.erase(remove(url.begin(),url.end(),'\''),url.end());
        //去除不包含//的地址类型，如：#default#homepage
        if(url.find("//") == np){
            begin = end+1;
            continue;
        } 
        //保留//之后的地址
        url = url.substr(url.find("//")+2);        
        //排除重复元素
        for(auto elmt:url_list)
            if(elmt == url) cnt++;
        if(cnt==0)
            url_list.push_back(url);
        //重置重复计数cnt和查找位置begin
        cnt = 0;
        begin = end+1;
    }
    return url_list;
}

void writeLocalFile(const string &content,const string &filename,const string &downpath/*=defDownPath*/){
    string filepath = downpath + '/' + filename;
    ofstream outfile(filepath,fstream::out);
    if(!outfile.is_open()){
        cout << "file open error\n";
        outfile.close();
        exit(-1);
    }
    outfile << content;
    outfile.close();
}

void writeLocalFile(const list<string> &strlist,const string &filename,const string &downpath/*=defDownPath*/){
    string filepath = downpath + '/' + filename;
    ofstream outfile(filepath,fstream::out);
    if(!outfile.is_open()){
        cout << "file open error\n";
        outfile.close();
        exit(-1);
    }
    for(auto str:strlist)
        outfile << str << endl;
    outfile.close();
    cout << "created file: " << filepath << endl << endl;
}

void writeLocalFile(const vector<char> &vcontent,const string &filename,const string &downpath/*=defDownPath*/,const char* prefix/*=""*/){
    string filepath = downpath + '/' + filename;
    ofstream outfile(filepath,fstream::out|fstream::binary);
    if(!outfile.is_open()){
        cout << "file open error\n";
        outfile.close();
        exit(-1);
    }
    //图片格式中包含的\0都是有意义的，一个都不能少
    for(int i=0;i<vcontent.size();i++)
        outfile << vcontent[i];
    outfile.close();
	cout << prefix << "created file: " << filepath << endl;
}
