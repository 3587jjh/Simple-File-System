#include <iostream>
#include <cstdio>
#include <vector>
#include <set>
#include <string>
#include <cstring>
using namespace std;

const int USERID = 2016147515;
const int MAX_INODE_NUM = 128;
const int BLOCKSIZE = 1024;
const int MAXBLOCK = 973;
int totBlock; // 현재 사용중인 block 개수
int totFile; // 현재 사용중인 file 개수

/**************************************************************************/
struct Inode {
	int id; // Inode ID
	string name; // file 이름
	int size; // file 사이즈, byte단위
	int fileBlock; // file이 차지하는 disk블록 개수
	int realBlock; // direct/indirect block까지 고려한 disk블록 개수
	int x; // direct blocks
	int y; // single indirect blocks
	int z; // double indirect blocks

	Inode() {}
	Inode(int _id, const string& _name, int _size) {
		id = _id;
		name = _name;
		size = _size; // size > 0
		fileBlock = (size-1)/BLOCKSIZE + 1;
		
		// direct blocks
		x = min(12, fileBlock);
		// single indirect blocks
		y = 0;
		if (fileBlock > 12)
			y = 1 + min(fileBlock-12, 256);	
		// double indirect blocks
		z = 0;
		if (fileBlock > 268) {
			int leftBlock = fileBlock-268;
			z = (leftBlock-1)/256 + 2 + leftBlock;
		}
		realBlock = x+y+z;
	}
};
// iUsed[i] = (i)번 inode ID가 사용중인가
bool iUsed[MAX_INODE_NUM];
Inode table[MAX_INODE_NUM];

/**************************************************************************/
struct Dentry {
	string name; // dentry 이름
	string path; // 절대 경로
	Dentry* par; // parent dentry 주소, NULL이면 root라는 뜻
	vector<Dentry*> children; // child dentry 주소 모음
	set<int> fid; // file의 inode ID 모음

	Dentry() : par(NULL) {}

	/**********************************/
	bool canCreateFile(const string& _name, int size) {
		// 파일 이름이 겹치면 에러
		set<int>::iterator it;
		for (it = fid.begin(); it != fid.end(); ++it) {
			int id = *it; // iUsed[id] = true임이 보장됨
			if (table[id].name == _name) return false;
		}
		// inode table 공간이 없으면 에러
		if (totFile == MAX_INODE_NUM) return false;
		// disk 공간이 없으면 에러, 파일 사이즈 > 0임이 보장됨
		Inode tmp(-1, _name, size);
		return totBlock + tmp.realBlock <= MAXBLOCK;
	}
	// 파일(_name)을 생성한다
	void createFile(const string& _name, int size) {
		int id = 0; // inode ID, 미사용중인 id가 있음이 보장됨
		while (iUsed[id]) ++id;

		Inode add(id, _name, size);
		iUsed[id] = true;
		table[id] = add;
		fid.insert(id);
		// 남은 disk 공간이 있음이 보장됨
		totBlock += add.realBlock;
		totFile += 1;
	}

	/**********************************/
	bool canRemoveFile(const string& _name) {
		// 파일 이름이 없으면 에러
		set<int>::iterator it;
		for (it = fid.begin(); it != fid.end(); ++it) {
			int id = *it;
			if (table[id].name == _name) return true;
		}
		return false;
	}
	// (id)의 inode ID를 가진 파일을 지운다
	void removeFile(int id) {
		// id는 사용중인 inode ID임이 보장됨
		totBlock -= table[id].realBlock;
		totFile -= 1;
		iUsed[id] = false;
		fid.erase(id);
	}
	// _name의 이름을 가진 파일을 지운다
	void removeFile(const string& _name) {
		set<int>::iterator it = fid.begin();
		while (table[(*it)].name != _name) ++it;
		// 지울 파일이 있음이 보장됨
		removeFile(*it);
	}

	/**********************************/
	bool canCreateDir(const string& _name) {
		// 디렉토리 이름이 겹치면 에러
		for (int i = 0; i < children.size(); ++i) {
			Dentry* child = children[i];
			if (child->name == _name) return false;
		}
		return true;
	}
	// 디렉토리(_name)를 생성한다
	void createDir(const string& _name) {
		Dentry* child = new Dentry();
		child->name = _name;
		child->path = path + (par == NULL ? "" : "/") + _name;
		child->par = this;
		children.push_back(child);
	}


	/**********************************/
	bool canRemoveDir(const string& _name) {
		// 디렉토리 이름이 없으면 에러
		for (int i = 0; i < children.size(); ++i) {
			Dentry* child = children[i];
			if (child->name == _name) return true;
		}
		return false;
	}
	// 현재 디렉토리에 있는 모든 디렉토리, 파일을 삭제한다
	void makeEmptyDir() {
		// 모든 자식 파일 삭제
		while (fid.begin() != fid.end())
			removeFile(*fid.begin());
		// 모든 자식 디렉토리 삭제
		while (!children.empty()) {
			Dentry* child = children.back();
			children.pop_back();
			child->makeEmptyDir();
			delete child;
		}
	}
	// 디렉토리(_name)을 삭제한다
	void removeDir(const string& _name) {
		// _name 디렉토리가 있음이 보장됨
		int i = 0;
		while (children[i]->name != _name) ++i;
		Dentry* child = children[i];
		children.erase(children.begin() + i);
		child->makeEmptyDir();
		delete child;
	}


	/**********************************/
	// _name 파일에 해당하는 Inode를 반환
	Inode getInode(const string& _name) {
		// 해당 파일이 있음이 보장됨
		set<int>::iterator it;
		for (it = fid.begin(); it != fid.end(); ++it) {
			int id = *it;
			if (table[id].name == _name)
				return table[id];
		}
	}
};

/**************************************************************************/
// ch문자를 기준으로 line의 단어들을 분리한다
vector<string> split(const string& line, char ch) {
	vector<string> ret;
	int l = 0;

	while (true) {
		while (l < line.size() && line[l] == ch) ++l;
		if (l == line.size()) break;
		int r = l;
		while (r < line.size() && line[r] != ch) ++r;
		ret.push_back(line.substr(l, r-l));
		l = r;
	}
	return ret;
}

/**************************************************************************/
int main() {
	// 처음 시작 위치는 루트 디렉토리
	Dentry* root = new Dentry();
	root->path = "/";
	Dentry* cur = root;

	while (true) {
		// 사용자 입력 대기
		printf("%d:%s$ ", USERID, cur->path.c_str());
		// 사용자 명령 입력
		string line;
		getline(cin, line);
		vector<string> input = split(line, ' ');
		if (input.empty()) continue; // 그냥 엔터만 친 경우
			
		if (input[0] == "ls") {
			// 디렉토리 출력
			for (int i = 0; i < cur->children.size(); ++i)
				cout << cur->children[i]->name << ' ';
			// 파일 출력
			set<int>::iterator it;
			for (it = cur->fid.begin(); it != cur->fid.end(); ++it)
				cout << table[(*it)].name << ' ';
			cout << "\n";
		}
		else if (input[0] == "cd") {
			// v = 이동하는 디렉토리 이름을 루트에서부터 순서대로 나열
			vector<string> v;
			vector<string> tmp = split(input[1], '/');

			// 인자가 절대경로인 경우
			if (input[1][0] == '/') v = tmp;
			else { // 인자가 상대경로인 경우
				v = split(cur->path, '/');
				for (int i = 0; i < tmp.size(); ++i)
					v.push_back(tmp[i]);
			}
			Dentry* prev = cur; // 현재 위치 저장해두기
			bool valid = true; // v의 순서대로 따라갈 수 있는가
			cur = root;

			for (int i = 0; i < v.size(); ++i) {
				string name = v[i];
				if (name == ".") continue;
				if (name == "..") {
					if (cur->par != NULL) cur = cur->par;
					continue;
				}
				// name 디렉토리가 존재하지 않는 경우
				if (!cur->canRemoveDir(name)) {
					valid = false;
					break;
				}
				// 해당 디렉토리로 이동한다
				for (int j = 0; j < cur->children.size(); ++j) {
					Dentry* child = cur->children[j];
					if (child->name == name) {
						cur = child;
						break;
					}
				}
			}
			// 따라가는데 실패한 경우
			if (!valid) {
				cout << "error\n";
				cur = prev; // 원래 위치 복원
			}
		}
		else if (input[0] == "mkdir") {
			bool valid = true; // 모든 디렉토리를 생성할 수 있는가
			for (int i = 1; i < input.size(); ++i) {
				string name = input[i];
				if (!cur->canCreateDir(name)) {
					valid = false;
					break;
				}
			}
			if (valid) { // 모든 디렉토리 생성하기
				for (int i = 1; i < input.size(); ++i) {
					string name = input[i];
					cur->createDir(name);
				}
			}
			else cout << "error\n";
		}
		else if (input[0] == "rmdir") {
			bool valid = true; // 모든 디렉토리를 지울 수 있는가
			for (int i = 1; i < input.size(); ++i) {
				string name = input[i];
				if (!cur->canRemoveDir(name)) {
					valid = false;
					break;
				}
			}
			if (valid) { // 모든 디렉토리 지우기
				for (int i = 1; i < input.size(); ++i) {
					string name = input[i];
					cur->removeDir(name);
				}
				// 정보 출력
				printf("Now you have ...\n");
				printf("%d / %d (blocks)\n", MAXBLOCK-totBlock, MAXBLOCK);
			}
			else cout << "error\n";
		}
		else if (input[0] == "mkfile") {
			string name = input[1];
			int size = stoi(input[2]);
			if (!cur->canCreateFile(name, size))
				cout << "error\n";
			else {
				cur->createFile(name, size);
				// 정보 출력
				printf("Now you have ...\n");
				printf("%d / %d (blocks)\n", MAXBLOCK-totBlock, MAXBLOCK);
			}
		}
		else if (input[0] == "rmfile") {
			bool valid = true; // 모든 파일을 지울 수 있는가
			for (int i = 1; i < input.size(); ++i) {
				string name = input[i];
				if (!cur->canRemoveFile(name)) {
					valid = false;
					break;
				}
			}
			if (valid) { // 모든 파일 지우기
				for (int i = 1; i < input.size(); ++i) {
					string name = input[i];
					cur->removeFile(name);
				}
				// 정보 출력
				printf("Now you have ...\n");
				printf("%d / %d (blocks)\n", MAXBLOCK-totBlock, MAXBLOCK);
			}
			else cout << "error\n";
		}
		else if (input[0] == "inode") {
			string name = input[1];
			// 해당 파일 이름이 없는 경우
			if (!cur->canRemoveFile(name)) {
				cout << "error\n";
				continue;
			}
			Inode A = cur->getInode(name);
			// 정보 출력
			printf("ID: %d\nName: %s\nSize: %d (bytes)\n",
				A.id, A.name.c_str(), A.size);
			printf("Direct blocks: %d\n", A.x);
			printf("Single indirect blocks: %d\n", A.y);
			printf("Double indirect blocks: %d\n", A.z);
		}
		else if (input[0] == "exit") {
			break; // terminate shell
		}
	}
	// 메모리 할당 해제
	root->makeEmptyDir();
	root = cur = NULL;
}
