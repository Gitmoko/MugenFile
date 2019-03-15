#ifndef SffdecH
#define SffdecH
#include <map>
#include <functional>
#include <iostream>
#include <fstream>
#include<iterator>

namespace{
	int place[4] = { 1, 0x100, 0x10000, 0x1000000 };
}


namespace Sffdec {

	struct Color {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t reserved;//pcxには無く、bmpにはある
		Color() {
			r = 0;
			g = 0;
			b = 0;
			reserved = 0;
		}
	};
	struct Palette {
		Color colors[256];

		Palette() :colors{} {}
	};

	constexpr int upperendian = 0x100;//エンディアンに気を付ける

	template <class T, class Arr>
	int push(Arr& tempbmp, int& tempit, T data) {//dataをbyte列にしてtempbmp[tempit]に押し込む。

		int size = sizeof(data);
		for (int i = 0; i < size; i++) {
			tempbmp[tempit] = (data >> 8 * i) % (0x100);
			tempit++;
		}
		return 0;
	}

	struct Pcxfileheader {
		uint8_t  Identifier;
		uint8_t  Version;
		uint8_t  Encoding;
		uint8_t  BitsPerPixel;
		uint16_t XStart;
		uint16_t YStart;
		uint16_t XEnd;//重要
		uint16_t YEnd;//
		uint16_t HorizontalResolution;
		uint16_t VerticalResolution;
		uint8_t  EGAPalette[48];
		uint8_t  Reserved1;
		uint8_t  NumBitPlanes;
		uint16_t BytesPerScanline;
		uint16_t PaletteType;
		uint16_t HorizontalScreenSize;
		uint16_t VerticalScreenSize;
		uint8_t  Reserved2[54];
	};

	//ヘッダ全体で１２８バイト
	//ていうか重要なのはｂｍｐファイルを作る方

	struct BMPINFOHEADER {
		uint32_t  biSize;//40にする
		uint32_t   biWidth;//適宜
		uint32_t   biHeight;//適宜
		uint16_t   biPlanes;//１とする
		uint16_t   biBitCount;//８とする
		uint32_t  biCompression;//0とする
		uint32_t  biSizeImage;//0とする
		uint32_t   biXPelsPerMeter;//どうでもいい
		uint32_t   biYPelsPerMeter;//どうでもいい
		uint32_t  biClrUsed;//256とする
		uint32_t  biClrImportant;//0とする

		BMPINFOHEADER() {
			biSize = 40;


			biPlanes = 1;
			biBitCount = 8;
			biCompression = 0;
			biSizeImage = 0;
			biXPelsPerMeter = 0;
			biYPelsPerMeter = 0;
			biClrUsed = 256;
			biClrImportant = 0;
		}
	};
	struct BMPFILEHEADER {
		uint16_t    bfType;//charで'B''M'バイト順に
		uint32_t   bfSize;//適宜
		uint16_t    bfReserved1;//0
		uint16_t    bfReserved2;//0
		uint32_t   bfOffBits;//0でもいい(閲覧ソフトによっては化けるので厳密にします)

		BMPFILEHEADER() {
			bfType = 'M' * 0x100 + 'B';

			bfReserved1 = 0;
			bfReserved2 = 0;

		}
	};



	inline uint8_t* Convert(uint8_t* (&pcxit), int& outputsizebuf, Palette* sharepalette, Palette* output_usedpalette, unsigned int* outputxsize, unsigned int* outputysize) {


		//まずはpcxを読み込む
		//ヘッダ読み込み
		if (uint8_t(*(pcxit++)) != 0x0A)//pcxデータでなければ
			return{};///////////
		for (int i = 0; i < 7; i++) {
			*(pcxit++);//ここまででXENDの先頭
		}

		int pcxwidth = uint8_t(*(pcxit++));
		pcxwidth += (uint8_t(*(pcxit++)))*upperendian + 1;//int pcxwidth = (uint8_t(*(pcxit++))) + uint8_t(*(pcxit++))*upperendian + 1;この場合, debug　と　release　で　計算順番が違う
		int pcxheight = (uint8_t(*(pcxit++)));
		pcxheight += uint8_t(*(pcxit++))*upperendian + 1;

		if (outputxsize != nullptr)
			*outputxsize = pcxwidth;
		if (outputysize != nullptr)
			*outputysize = pcxheight;

		for (int i = 0; i < 116; i++) {
			*(pcxit++);
		}//これでヘッダ終了

		//インデックス読み込みrle圧縮に気を付けよう

		std::vector<std::vector<uint8_t>> pcxindex(pcxheight, std::vector<uint8_t>(pcxwidth, 0));

		for (auto h = pcxindex.begin(); h != pcxindex.end(); h++) {
			for (auto w = (*h).begin(); w != (*h).end();) {
				int temp = uint8_t(*(pcxit++));
				if (temp > 0xC0) {//rle圧縮なら
					uint8_t times = temp - 0xC0;
					uint8_t seq_index = uint8_t(*(pcxit++));
					for (int i = 0; i < times; i++) {
						(*w) = (seq_index);
						w++;
					}
				}
				else {
					(*w) = (temp);
					w++;
				}
			}
		}

		//パレットとの境界0x0Cを読み込む
		if (uint8_t(*(pcxit++)) != 0x0C)//0x0Cになってる？
			return{};////////

		////独立パレットの場合,パレット読み込み
		Palette* mypalette = nullptr;
		if (sharepalette == nullptr) {
			mypalette = new Palette();

			for (int i = 0; i < 256; i++) {
				if (i == 0) {//インデックス０番は透過として扱う.DxLibはデフォルトでは(0,0,0)を透過色で扱うから
					mypalette->colors[i].r = 0;
					mypalette->colors[i].g = 0;
					mypalette->colors[i].b = 0;

					for (int i = 0; i < 3; i++)
						*(pcxit++);
				}
				else {
					mypalette->colors[i].r = uint8_t(*(pcxit++));
					mypalette->colors[i].g = uint8_t(*(pcxit++));
					mypalette->colors[i].b = uint8_t(*(pcxit++));
				}
			}
		}
		//必要に応じて使用したパレットを返す
		if (output_usedpalette != nullptr) {
			if (sharepalette == nullptr) {
				for (int i = 0; i < 256; i++) {
					output_usedpalette->colors[i].r = mypalette->colors[i].r;
					output_usedpalette->colors[i].g = mypalette->colors[i].g;
					output_usedpalette->colors[i].b = mypalette->colors[i].b;
				}
			}
			else {
				for (int i = 0; i < 256; i++) {
					output_usedpalette->colors[i].r = sharepalette->colors[i].r;
					output_usedpalette->colors[i].g = sharepalette->colors[i].g;
					output_usedpalette->colors[i].b = sharepalette->colors[i].b;
				}
			}
		}

		/*if (non_comvert_ptob){
			//インデックス
			for (int h = bmptag.biHeight - 1; h != -1; h--){
				for (unsigned int w = 0; w < bmptag.biWidth; w++){
					push(bmpdata, it, pcxindex[h][w]);
				}
				for (int w = 0; w < resizeline; w++){
					push(bmpdata, it, uint8_t(0));
				}
			}
			return;
		}*/

		//bmpファイルヘッダを作る
		BMPFILEHEADER bmpfile;//ここで全データ量がbfSizeに必要だが８３行の直下でする

		//bmp画像ヘッダを作る
		BMPINFOHEADER bmptag;
		bmptag.biHeight = pcxheight;
		bmptag.biWidth = pcxwidth;
		//メモリにbmpデータを展開していく

		int resizeline = (4 - (bmptag.biWidth) % 4) % 4;//何マス入れねばならないか
		int bmpline = bmptag.biWidth + resizeline;//予想されるbmpの横幅
		uint32_t bmpsize = (14) + (bmptag.biSize) + (bmptag.biClrUsed * 4) + (bmpline*bmptag.biHeight);//ファイルヘッダ、画像ヘッダ、パレット、補完した画像のピクセル数 単位はbyte
		outputsizebuf = bmpsize;
		bmpfile.bfSize = bmpsize;


		uint8_t* bmpdata = new uint8_t[bmpsize];
		int it = 0;
		//ファイルヘッダ
		push(bmpdata, it, bmpfile.bfType);
		push(bmpdata, it, bmpfile.bfSize);
		push(bmpdata, it, bmpfile.bfReserved1);
		push(bmpdata, it, bmpfile.bfReserved2);
		//オフセットを求める。全サイズからインデックスのサイズを引く 0 1 2 3 4
		bmpfile.bfOffBits = bmpsize - (bmpline*bmptag.biHeight);//   a a a b b    5-2 = 3
		push(bmpdata, it, bmpfile.bfOffBits);
		//画像ヘッダ
		push(bmpdata, it, bmptag.biSize);
		push(bmpdata, it, bmptag.biWidth);
		push(bmpdata, it, bmptag.biHeight);
		push(bmpdata, it, bmptag.biPlanes);
		push(bmpdata, it, bmptag.biBitCount);
		push(bmpdata, it, bmptag.biCompression);
		push(bmpdata, it, bmptag.biSizeImage);
		push(bmpdata, it, bmptag.biXPelsPerMeter);
		push(bmpdata, it, bmptag.biYPelsPerMeter);
		push(bmpdata, it, bmptag.biClrUsed);
		push(bmpdata, it, bmptag.biClrImportant);
		//カラーパレット,bmpのデータの並びはb,g,rであることに注意

		if (sharepalette == nullptr) {
			for (int i = 0; i < 256; i++) {
				push(bmpdata, it, mypalette->colors[i].b);
				push(bmpdata, it, mypalette->colors[i].g);
				push(bmpdata, it, mypalette->colors[i].r);
				push(bmpdata, it, mypalette->colors[i].reserved);
			}
		}
		else {
			for (int i = 0; i < 256; i++) {
				if (i == 0) {
					push(bmpdata, it, (uint8_t)0);//このキャストは極めて重要
					push(bmpdata, it, (uint8_t)0);
					push(bmpdata, it, (uint8_t)0);
					push(bmpdata, it, (uint8_t)0);
				}
				else {
					push(bmpdata, it, sharepalette->colors[i].b);
					push(bmpdata, it, sharepalette->colors[i].g);
					push(bmpdata, it, sharepalette->colors[i].r);
					push(bmpdata, it, (uint8_t)0);
				}
			}
		}

		//インデックス
		for (int h = bmptag.biHeight - 1; h != -1; h--) {
			for (unsigned int w = 0; w < bmptag.biWidth; w++) {
				push(bmpdata, it, pcxindex[h][w]);
			}
			for (int w = 0; w < resizeline; w++) {
				push(bmpdata, it, uint8_t(0));
			}
		}

		if (mypalette != nullptr)
			delete mypalette;

		return bmpdata;
	}


	template<class Data_t>
	struct GDATA
	{
		short revisionx;
		short revisiony;

		unsigned int  xsize;
		unsigned int  ysize;

		Data_t ghandle;
		GDATA() {

			revisionx = 0;
			revisiony = 0;

			xsize = 0;
			ysize = 0;

		}
	};

	typedef short GROUP;
	typedef unsigned short IMAGE;

	//using Data_t = int;
	template<class Data_t>
	class SSff {
	public:
		SSff() {}
		SSff(SSff<Data_t> && right) : Graphdata(std::move(right.Graphdata)) {}
		SSff(std::string filename, std::function<Data_t(uint8_t* (&bmpdata), const int& sizebuf)> func) {
			Decord(filename, func);
		}
	public:
		std::map < GROUP, std::map<IMAGE, GDATA<Data_t> > > Graphdata;
		int Decord(std::string filename, std::function<Data_t(uint8_t* (&bmpdata), const int& sizebuf)> func) {

			std::ifstream sff(filename, std::ios::binary);
			if (!sff) {
				return -1;///////////////////
			}

			sff.seekg(0, std::ifstream::end);//ファイル末尾を探す
			
			auto eofPos = sff.tellg();//ファイル末尾インデクスを取得
			sff.clear();//先頭にもどるために一度clear()をかける。これをしないと次のseekg()でコケるときがある。
			sff.seekg(0, std::ifstream::beg);//ファイル先頭に戻る
			auto begPos = sff.tellg();//ファイル先頭インデクスを取得
			auto size = eofPos - begPos;

			auto it = new uint8_t[size];
			auto itbeg = it;

			sff.read(reinterpret_cast<char*>(it),size);
			sff.clear();//先頭にもどるために一度clear()をかける。これをしないと次のseekg()でコケるときがある。
			sff.seekg(0, std::ifstream::beg);//ファイル先頭に戻る


			int numofgroupes = 0;
			int numofimages = 0;

			for (int i = 0; i < 0x10; i++)//グループ数まで飛ぶ
				std::uint8_t(*(it++));

			for (int i = 0; i < sizeof(numofgroupes); i++) {//groupe
				numofgroupes += std::uint8_t(*(it++)) * place[i];
			}
			for (int i = 0; i < sizeof(numofimages); i++) {//Image
				numofimages += std::uint8_t(*(it++)) * place[i];
			}

			{
				{
					long long now = it - itbeg;
					for (int i = 0; i < 0x200 - now; i++) {//最初の画像ヘッダへ
						std::uint8_t(*(it++));
					}
				}

				Palette firstpalette;//Group0,Image0の画像がパレット共有の場合、一つ上のパレットでなく一番最初にある画像のパレットを参照するらしいので、最初のパレットを保持する必要がある
				Palette sharepalette;//独立パレット画像を読み込むたびに変更する
				std::vector<std::pair<Sffdec::GROUP, Sffdec::IMAGE>> order;//入れた順番にグループ、イメージ値を入れる

				for (int i = 0; i < numofimages; i++) {
					unsigned int nextgraphheader = 0;
					bool shareflag = false;
					bool renewflag = true;
					short cloneflag = 0;
					uint8_t* bmpdata = nullptr;
					GDATA<Data_t> Data;
					GROUP Group = 0;
					IMAGE Image = 0;
					for (int j = 0; j < sizeof(nextgraphheader); j++) {
						nextgraphheader += std::uint8_t(*(it++)) * place[j];//次の画像ヘッダアドレスoffset
					}
					for (int j = 0; j < 4; j++)//pcxデータサイズ
						std::uint8_t(*(it++));
					for (int j = 0; j < sizeof(Data.revisionx); j++) {//x補正
						Data.revisionx += std::uint8_t(*(it++))*place[j];
					}
					for (int j = 0; j < sizeof(Data.revisiony); j++) {//ｙ補正
						Data.revisiony += std::uint8_t(*(it++))*place[j];
					}
					for (int j = 0; j < sizeof(Group); j++) {
						Group += std::uint8_t(*(it++))*place[j];
					}
					for (int j = 0; j < sizeof(Image); j++) {
						Image += std::uint8_t(*(it++))*place[j];
					}

					for (int j = 0; j < sizeof(cloneflag); j++) {/*クローンフラグデータは2byte使われていて、上から何番目の画像と一致
																 しているかかが入る。クローンでないときは０しかし、０番目の画像と一致しているときも０が入るらしいが、どうやって
																 区別しているかは不明*/
						cloneflag += std::uint8_t(*(it++))*place[j];
					}

					if (i == 0) {//0枚目の共有パレットフラグは無視
						std::uint8_t(*(it++));
						shareflag = false;
						renewflag = true;
					}
					else {
						shareflag = bool(std::uint8_t(*(it++)) == 1);//共有
						if (shareflag) {
							renewflag = false;
							if (Group == 0 && Image == 0)//Group0,Image0の共有フラグが立っているとき、sffの先頭画像のパレットデータが使われる
								sharepalette = firstpalette;
						}
						else
							renewflag = true;
					}

					for (int j = 0; j < 0x20 - 0x13; j++) {//使われていない領域をスルー
						std::uint8_t(*(it++));
					}

					if (cloneflag > 0) {//cloneがあるとき
						if (nextgraphheader != static_cast<long long>(it-itbeg)) {
							return -2;//////////
						}
						else {
							auto orderpair = order[cloneflag];
							if (Graphdata.find(Group) == Graphdata.end()) {//そのグループがなければ
								Graphdata.insert(std::map<GROUP, std::map<IMAGE, GDATA<Data_t>> >::value_type(Group, {}));
							}

							Graphdata[Group].insert({ Image, Graphdata[orderpair.first][orderpair.second] });

							order.push_back({ Group, Image });
						}
					}
					else {//cloneがないとき

						int sizebuf;
						bmpdata = Convert(it, sizebuf, shareflag == true ? &sharepalette : nullptr, renewflag == true ? &sharepalette : nullptr, &Data.xsize, &Data.ysize);
						if (bmpdata == 0) {
							return -3;//////////
						}
						Data.ghandle = func(bmpdata, sizebuf);
						delete[](bmpdata);

						std::pair<IMAGE, GDATA<Data_t>> temppair(Image, Data);
						if (Graphdata.find(Group) == Graphdata.end()) {//そのグループがなければ
							Graphdata.insert({ Group,{} });
						}

						Graphdata[Group].insert(temppair);
						order.push_back({ Group, Image });

						if (i == 0)
							firstpalette = sharepalette;
					}
					if (nextgraphheader != static_cast<long long>(it-itbeg)) {
						return -2;//////////
					}
				}
			}
			delete[] itbeg;
			return 0;
		}

		//pfunc: bmpdataにはいったbmp画像のデータから画像を生成する関数。bmpdataは勝手にdeleteしないで.
		/*DxLibをつかう時の実装例
		**************************************************
		int user_func(const char* data, int&size){
		int ghandle = CreateGraphFromMem((char*)bmpdata, size);
		return ghandle;
		}

		int main(){
		SSff sff("Data/filename.sff",user_func);
		return 0;
		}
		***************************************************


		*/
	};
	template<class Data_t>
	class CSffmgr {

		static constexpr unsigned int bufsize = 50000000;
		std::map<std::string, SSff<Data_t>> data;
	private:

		CSffmgr() {}
		CSffmgr(CSffmgr<Data_t>&) {}
		CSffmgr(CSffmgr<Data_t>&&) {}
	public:

		static CSffmgr& GetSffmgr() {
			static CSffmgr<Data_t> stdata;
			return stdata;
		}
		~CSffmgr() {}
	public:
		int insertsff(std::string filename, std::function<Data_t(uint8_t* (&bmpdata), const int& sizebuf)> func) {
			if (data.count(filename) > 0)
				return -1;

			data.emplace(std::pair < std::string, SSff<Data_t> > {filename, SSff<Data_t>{filename, func}});
			return 0;
		}
		int insertsff(std::string filename) {
			if (data.count(filename) > 0)
				return -1;
			data.insert(decltype(data)::value_type{ filename,SSff<Data_t>{} });
			return 0;
		}
		int erasesff(std::string filename) {
			if (data.count(filename) > 0)
				data.erase(filename);

			return 0;
		}
		int eraseall() {
			data.clear();
			return 0;
		}
		void aplly(std::function<void(SSff<Data_t>&)>func) {
			for (auto&e : data) {
				func(e.second);
			}
		}
		bool count(std::string filename) {
			auto ret = data.count(filename) > 0;
			return ret;
		}
		SSff<Data_t>& operator[](std::string filename) {
			return data[filename];
		}


	};




}



#endif