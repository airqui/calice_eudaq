// ScReader.cc
#include "ScReader.hh"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

using namespace eudaq;
using namespace std;

namespace calice_eudaq {
  void ScReader::OnStart(int runNo){
    _runNo = runNo;
    _cycleNo = -1;
    _tempmode = false;
    
    // set the connection and send "start runNo"
    _receiver->OpenConnection();

    /*
    char s[9];
    const char *msg = "START";
    strcpy(s, msg);
    unsigned int run = runNo;
    char *ss = (char *)&run;
    memcpy(s + 5, ss, 4);
    */

    // using characters to send the run number
    ostringstream os;
    os << "START";
    os.width(8);
    os.fill('0');
    os << runNo;
    
    _receiver->SendCommand(os.str().c_str());
  }

  void ScReader::OnStop(){
    const char *msg = "STOP";
    _receiver->SendCommand(msg);
    
    //_receiver->CloseConnection();
  }

  void ScReader::Read(std::deque<char> & buf, std::deque<eudaq::RawDataEvent *> & deqEvent)
  {
    // temporary output buffer
    vector<vector<int> > outbuf;
    
    try{
    while(1){
      unsigned char magic[2] = {0xcd, 0xcd};
      int ndrop = 0;
      while(buf.size() > 1 && ((unsigned char)buf[0] != magic[0] || (unsigned char)buf[1] != magic[1])){
        buf.pop_front();ndrop ++;
      }
      if(ndrop>0)cout << "ScReader::Read(); " << ndrop << " bytes dropped before SPILL header marker found." << endl;
      if(buf.size() <= e_sizeLdaHeader) throw 0; // all data read

      unsigned int length = (((unsigned char)buf[3] << 8) + (unsigned char)buf[2]);//*2;
      cout << "ScReader::Read(): length = " << length << endl;

      if(buf.size() <= e_sizeLdaHeader + length) throw 0;
      unsigned int cycle = (unsigned char)buf[4];

      // we currently ignore packet rather than SPIROC data
      unsigned char status = buf[9];
      cout << "status = " << std::hex << status << std::dec << endl;
      
      // for the temperature data we should ignore the cycle # because it's invalid.
      bool tempcome = (status == 0xa0 && buf[10] == 0x41 && buf[11] == 0x43 && buf[12] == 0x7a && buf[13] == 0);
      
      while(deqEvent.size() == 0 || (!tempcome && ((_cycleNo + 256) % 256) != cycle)){
        // new event arrived: create RawDataEvent
        _cycleNo ++;
        
        cout << "New acq ID " << _cycleNo << " added." << endl;
        RawDataEvent *nev = new RawDataEvent("CaliceObject", _runNo, _cycleNo);
        string s = "ScECAL";
        nev->AddBlock(0,s.c_str(), s.length());
//        s = "i:cycle,i:bx,i:chipid,i:mem,i:cell,i:adc,i:tdc,i:trig,i:gain";
        // Changed! 141203
        s = "i:CycleNr;i:BunchXID;i:ChipID;i:EvtNr;i:Channel;i:ADC;i:TDC;i:HitBit;i:GainBit";
        nev->AddBlock(1,s.c_str(), s.length());
        unsigned int times[2];
        struct timeval tv;
        ::gettimeofday(&tv, NULL);
        times[0] = tv.tv_sec;
        times[1] = tv.tv_usec;
        nev->AddBlock(2, times, sizeof(times));
        nev->AddBlock(3, vector<int>()); // dummy block to be filled later

        deqEvent.push_back(nev);
      }

      cout << "cycle # = " << _cycleNo << endl;

      if(status == 0xa0 && buf[10] == 0x41 && buf[11] == 0x43 && buf[12] == 0x7a && buf[13] == 0){
        _tempmode = true;

        cout << "DIF-ADC packet found." << endl;
        int lda = buf[6];
        int port = buf[7];
        short data = ((unsigned char)buf[23] << 8) + (unsigned char)buf[22];
        cout << "LDA " << lda << " PORT " << port << " DATA " << data << endl;
        
        _vecTemp.push_back(make_pair(make_pair(lda,port),data));
        
      } else if (_tempmode){
        // tempmode finished; store to the rawdataevent 
        RawDataEvent *ev = deqEvent.back();
        vector<int> output;
        for(unsigned int i=0;i<_vecTemp.size();i++){
          int lda,port,data;
          lda = _vecTemp[i].first.first;
          port = _vecTemp[i].first.second;
          data = _vecTemp[i].second;
          output.push_back(lda);
          output.push_back(port);
          output.push_back(data);
        }
        ev->AppendBlock(3, output);
        _tempmode = false;
        cout << "Temperature data of " << _vecTemp.size() << " elements are stored." << endl;
      }
      
      if(!(status & 0x40)){
	cout << "We'll drop non-data packet." << endl;
        
        for(unsigned int i=0;i<length+e_sizeLdaHeader;i++)
        {
          if(i%16==0) cout << std::hex << std::setw(8) << i << " ";
          cout << std::hex << (unsigned int)(unsigned char)buf[i];
          //if(i%2==1)
            cout << " ";
          if(i%16==15) cout << endl;
        }
        cout << std::dec << endl;

	// remove used buffer
	cout << "Removing " << length + e_sizeLdaHeader << " bytes from " << buf.size() << " bytes." << endl;
	buf.erase(buf.begin(), buf.begin() + length + e_sizeLdaHeader);
	cout << "Removed: " << buf.size() << " bytes remaining." << endl;
	continue;
      }

      RawDataEvent *ev = deqEvent.back();
      deque<char>::iterator it = buf.begin() + e_sizeLdaHeader;

      // 0x4341 0x4148
      if(it[1] != 0x43 || it[0] != 0x41 || it[3] != 0x41 || it[2] != 0x48){
	cout << "ScReader: header invalid." << endl;
	for(int i=0;i<4;i++){
	  cout << (int)it[i] << " ";
	}
	cout << endl;
	buf.pop_front();
	continue;
      }

      // chain #, asic #
      int chain = it[4];
      int asic = it[5];
      cout << "Chain # = " << chain << ", ASIC # = " << asic << endl;

      // DIF # = 0; nocheck

      // footer check: ABAB
      if((unsigned char)it[length-2] != 0xab || (unsigned char)it[length-1] != 0xab)
	cout << "Footer abab invalid:" << (unsigned int)(unsigned char)it[length-2] << " " << (unsigned int)(unsigned char)it[length-1] << endl;

      // just a check: no exeption

      int chipId = (unsigned char)it[length-3] * 256 + (unsigned char)it[length-4];
      cout << "chipid = " << chipId << endl;

      const int npixel = 36;
      int nscai = (length-8) / (npixel * 4 + 2);
      cout << "Trigger count = " << nscai << endl;

      it += 8;
      // list hits to add
      for(int tr=0;tr<nscai;tr++){
	// binary data: 128 words
	short adc[npixel], tdc[npixel];
	short trig[npixel], gain[npixel];
	for(int np = 0; np < npixel; np ++){
	  unsigned short data = (unsigned char)it[np * 2] + ((unsigned char)it[np * 2 + 1] << 8);
	  tdc[np]  = data % 4096;
	  gain[np] = data / 8192;
	  trig[np] = (data / 4096)%2;
	  unsigned short data2 = (unsigned char)it[np * 2 + npixel * 2] + ((unsigned char)it[np * 2 + 1 + npixel * 2] << 8);
	  adc[np]  = data2 % 4096;
	}
	it += npixel * 4;

	int bxididx = e_sizeLdaHeader + length - 4 - (nscai-tr) * 2;
	int bxid = (unsigned char)buf[bxididx + 1] * 256 + (unsigned char)buf[bxididx];
	for(int n=0;n<npixel;n++){
	  vector<int> data;
	  data.push_back((int)_cycleNo);
	  data.push_back(bxid);
	  data.push_back(chipId);
	  data.push_back(nscai - tr - 1);
	  data.push_back(npixel - n - 1);
	  data.push_back(adc[n]);
	  data.push_back(tdc[n]);
	  data.push_back(trig[n]);
	  data.push_back(gain[n]);
	
	  outbuf.push_back(data);
	}
      }
      // commit events
      if(outbuf.size()){
        for(unsigned int i=0;i<outbuf.size();i++){
          ev->AddBlock(ev->NumBlocks(), outbuf[i]);
        }
        outbuf.clear();
      }
      
      // remove used buffer
      cout << "Removing " << length + e_sizeLdaHeader << " bytes from " << buf.size() << " bytes." << endl;
      buf.erase(buf.begin(), buf.begin() + length + e_sizeLdaHeader);
      cout << "Removed: " << buf.size() << " bytes remaining." << endl;
    }
    }catch(int i){} // throw if data short
    
    cout << "Readout finished. remained bytes: " << buf.size() << endl;
  }
}
