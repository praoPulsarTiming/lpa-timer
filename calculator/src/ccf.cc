#include "ccf.h"
#include<iomanip>
#include<fstream>


Cor::Cor(){}; // конструктор
Cor::~Cor(){};

// кросс-корреляция данных  
//************************

void Cor::ccf(SumProfile finPulse, std::string rawdata_dir, std::string output_dir, std::string runs, std::string tplfile, float utcloc) 
// void Cor::ccf(SumProfile finPulse, ConfigParam conf, float utcloc) 
{
  
  //BaseRun br;
  
  std::string sdummy;
  int kkfyes,toayes,kkfdatyes; // =1 при успешной записи в файлы kkf и toa
  
  float dt = finPulse.tau;       // дискрет меду отсчетами в профиле
  
  long double mjd; 
  Tpl tpl, tplscaled;
  Skkf kkf;
  Itoa itoa;
  
  float dtkkf=0.;
  int kkflen;
  float dummy;
  float maxkkf;
  int maxi;
  float startdelay; // задержка после времени старта указанной в протоколе в мкс, для ЦПП-DPP1=1 дискрету
  float CMprf, CMtpl; // центры масс профиля и шаблона
  
  // * версия для работы с шаблоном фиксированного временного разрешения
  //kkf.numpoint=finPulse.numpointwin; // присвоение длины ккф в структуре
  //tpl = Tplread(tplfile);            // считывкание файла шаблона из файла 'tplfile'
  
  kkf.numpoint=finPulse.numpointwin; // присвоение длины ккф в структуре
  tpl = Tplread(tplfile);            // считывание файла шаблона из файла 'tplfile'
  if (dt!=tpl.tau) tplscaled=ScaleTpl(tpl,dt);         // приведение масштаба шаблона к масштабу данных, если необходимо
  if (dt!=tpl.tau) kkf.kkfdata=dccf(tplscaled,finPulse); else kkf.kkfdata=dccf(tpl,finPulse); // вычисление дискретной ККФ и запись в структуру Skkf
  
  //std::cout << "$$$$$$$$$$$$$$$$ шаблон успешно считан" << std::endl;  //test
  
  kkf.kkfdata=dccf(tpl,finPulse); // вычисление дискретной ККФ и запись в структуру Skkf
  
  //std::cout << "$$$$$$$$$$$$$$$$ дискретная ККФ успешно вычислена" << std::endl; //test
  
  maxkkf=kkf.kkfdata[0]; 
  maxi=0;
  for (int i=0; i<finPulse.numpointwin;i++) 
    if (maxkkf<=kkf.kkfdata[i]) 
      {
	maxkkf=kkf.kkfdata[i];
        maxi=i;
      };
  
  for (int i=0; i<5; i++) kkf.kk.push_back(kkf.kkfdata[maxi-2+i]);
  
  // std::cout << "$$$$$$$$$$$$$$$$ ККФ возле максимума успешно вычислена" << std::endl; //test
  
  //for(int i=0; i<finPulse.numpointwin;i++) std::cout << kkf.kkfdata[i] << std::endl;
  
  if (!strcmp(finPulse.rtype.c_str(),"DPP1")) startdelay=1000.*dt; else startdelay=0.;
  
  // std::cout << "$$$$$$$$$$$$$$$$ старт поиска ЦМ профиля" << std::endl; // test
  
  CMprf= CofM(finPulse.prfdata, finPulse.numpointwin, finPulse.tau);
  std::cout << "Центр Масс профиля:   " << CMprf << std::endl; 
  CMtpl = CofM(tpl.tpldata, tpl.numpoint, tpl.tau);
  std::cout << "Центр Масс шаблона:   " << CMtpl << std::endl; 
  
  
  if (CMprf >= CMtpl) kkf.maxp=startdelay+1000.*dt*(maxi+ApproxMax(kkf.kk[0], kkf.kk[1],kkf.kk[2],kkf.kk[3],kkf.kk[4]));
  else kkf.maxp=-1000.*finPulse.numpointwin*finPulse.tau+startdelay+1000.*dt*(maxi+ApproxMax(kkf.kk[0], kkf.kk[1],kkf.kk[2],kkf.kk[3],kkf.kk[4]));
  
  itoa.TOAMJD = utc2mjd(finPulse, utcloc, kkf.maxp); // расчет MJD.MJD и запись в структуру Itoa
  
  itoa.sMJD = utc2mjds(finPulse, utcloc, kkf.maxp);
  
  kkf.snr = SNR(finPulse);
  kkf.errmax = CalcErrorW50(finPulse, kkf.snr);
  //структура kkf заполнена
  
  itoa.psrname = finPulse.psrname;
  
  itoa.TOAerr = kkf.errmax;
  if (kkf.errmax<=999.) itoa.TOAerr = kkf.errmax; else itoa.TOAerr = 999.0; // чтобы не вылезать из формата itoa
  itoa.freq = finPulse.freq;
  itoa.ddm = 0.0; // поправка вносится в файл itoa только по многочастотным данным
  itoa.obscode = finPulse.obscode;
  
  
  std::cout << "ККФ:<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
  std::cout << "Значения ККФ в окрестности максимума:" << std::endl;
  
  printf ("%10.4f%10.4f%10.4f%10.4f%10.4f\n", kkf.kk[0],kkf.kk[1],kkf.kk[2],kkf.kk[3],kkf.kk[4]);
  std::cout << "\n  Max ККФ   мкс  ||  ошибка   мкс  || отношение сигнал/шум " << std::endl;
  printf ("%15.2f%15.2f%15.2f \n ", kkf.maxp, kkf.errmax, kkf.snr);
  
  std::cout << "МПИ:<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
  std::cout << "Пульсар |    МПИ(MJD)  | Err(мкс)| Частота(МГц)  | поправка DM(мкс) | Код обсерватории" << std::endl;
  printf("%-9s%19.13llf%6.2f%12.5f%9.6f%4s\n",itoa.psrname.c_str(),itoa.TOAMJD,itoa.TOAerr,itoa.freq,itoa.ddm,itoa.obscode.c_str());
  
  kkfyes = KKFWrite(output_dir, kkf);
  toayes=TOAWrite(output_dir, itoa);
  kkfdatyes=KKFdatWrite(output_dir, kkf, runs);

  
}; 
// end of function ccf
//**********************************



// функция расчета MJD.MJD из даты, возвращает mjd в формате long double
//******************************************************************************
// dtkkf - сдвиг профиля относительно шаблона (максимум ККФ) в микросекундах

long double Cor::utc2mjd(SumProfile finPulse, float utcloc, float dtkkf)
{
  int yy=2000+finPulse.utcyear; // yy+2000 - т.к. формат года в dutc - двузначный
  int mm=finPulse.utcmonth;
  int dd=finPulse.utcday;
  int hh=finPulse.utchour-utcloc;
  int min=finPulse.utcmin;
  long double ss=finPulse.utcsec;
  int imjd;
  long double fmjd;
  long double mjd;
  
  
  if (hh < 0) {hh+=24; dd-=dd;} // переход через сутки если по гринвичу не наступила текущая дата
  
  // усложненная процедура расчета чтобы избежать набега ошибки из-за нехватки разрядной сетки
  imjd=367*yy-7*(yy+(mm+9)/12)/4-3*((yy+(mm-9)/7)/100+1)/4 // расчет целого числа MJD
    +275*mm/9+dd+1721028-2400000;					 
  fmjd=hh/24.+min/1440.+ss/86400.+dtkkf/86400./1000000.; // расчет дробной части mjd до 13 знака
  
  mjd=imjd+fmjd;
  
  //printf ("MJD########## из utc2mjd: %25.13llf \n ", mjd);

  return mjd;
}

//******************************************************************************
// функция расчета MJD.MJD из даты, возвращает mjd в формате string

std::string Cor::utc2mjds(SumProfile finPulse, float utcloc, float dtkkf)
{
  int yy=2000+finPulse.utcyear; // yy+2000 - т.к. формат года в dutc - двузначный
  int mm=finPulse.utcmonth;
  int dd=finPulse.utcday;
  int hh=finPulse.utchour-utcloc;
  int min=finPulse.utcmin;
  long double ss=finPulse.utcsec;
  int imjd;
  long double fmjd;
  long double mjd;
  std::string smjd;
  char tmp[100];
  
  if (hh < 0) {hh+=24; dd-=dd;} // переход через сутки если по гринвичу не наступила текущая дата

  // усложненная процедура расчета чтобы избежать набега ошибки из-за нехватки разрядной сетки
  imjd=367*yy-7*(yy+(mm+9)/12)/4-3*((yy+(mm-9)/7)/100+1)/4 // расчет целого числа MJD
    +275*mm/9+dd+1721028-2400000;					 
  fmjd=hh/24.+min/1440.+ss/86400.+dtkkf/86400./1000000.; // расчет дробной части mjd до 13 знака
  
  mjd=imjd+fmjd;
  
  //smjd << mjd;
  //sprintf ("%25.13llf \n ",smjd.c_str(), mjd);
  
  sprintf(tmp,"%19.13llf",tmp,mjd);
  for (int i=0; i<19; i++) smjd.push_back(tmp[i]);
  smjd.push_back('\0');
  return smjd;
  
}

//Чтение файла шаблона. Возвращает шаблон в структуре Tpl
//******************************************************************************
//Cor::Tpl Cor::Tplread(std::string rawdata_dir)
Tpl Cor::Tplread(std::string tplfile)  
{
  //Cor::Tpl tpl;
  Tpl tpl;
  std::string tfilename=tplfile;
  std::ifstream itpl(tfilename.c_str());
  float dummy;
  std::string sdummy;
  
  if (!itpl) {std::cerr << "Не могу найти файл шаблона " <<  tfilename << "  аварийный выход " <<std::endl;  exit(0);}
  else {std::cout << "Шаблон считывается из файла " << tfilename << std::endl;};
  
  itpl >> tpl.psrname >> tpl.date >> tpl.freq >> tpl.tau >> tpl.numpoint >> sdummy; 
  // std::cout <<  tpl.psrname <<"\n" << tpl.date << "\n" << tpl.freq << "\n" << tpl.tau << "\n" << tpl.numpoint << "\n" << sdummy << "\n"; 
  for (int i=0;i<tpl.numpoint;i++) {itpl >> dummy; tpl.tpldata.push_back(dummy);};
  //std::cout << "Шаблон успешно считан из файла " << tfilename << std::endl;
  
  return tpl;
};
// Endof Tplread
//**********************************************************************************************

// Пересчет шаблона к временнОму разрешению вектора данных
//**********************************************************************************************
//Cor::Tpl ScaleTpl(Cor::Tpl intpl, float taus)
Tpl Cor::ScaleTpl(Tpl intpl, float taus) 
{
  Tpl tplnew; // новый шаблон, taus - новое разрешение шаблона
  int K,N,n;
  float scale, fp, dtaus, tau;
  tau=intpl.tau;
  N=intpl.numpoint; // число точек в старом шаблоне
  K = (int)(N*intpl.tau/taus); // число точек в новом шаблоне
  
  if (taus != tau)	// вычисление нового массива шаблона
    {
      for (int k=0; k<K;k++) 
	{
	  n =(int)(k*taus/tau);
	  //                std::cout << "k="<< k << " n=" << n;
	  dtaus = k*taus-n*tau;
	  //                std::cout << " dtaus ="<< dtaus;
	  tplnew.tpldata.push_back(intpl.tpldata[n]+(intpl.tpldata[n+1]-intpl.tpldata[n])/tau*dtaus);
	  //                std::cout << "  tpl" << "[" << n << "]=" <<  intpl.tpldata[n] << "  tpl" << "[" << k << "]="  << tplnew.tpldata[k] << std::endl;
	}
      tplnew.tpldata[K-1]=intpl.tpldata[N-1];
      for (int i=0; i<K;i++) std::cout << tplnew.tpldata[i] << std::endl;
      std::cout << std::endl;
    }
  tplnew.tau=tau;
  return tplnew;
};

// Endof ScaleTpl
//**********************************************************************************************

// Запись во внешний файл в формате itoa
//**********************************************************************************************

int Cor::TOAWrite(std::string output_dir, Itoa itoa)
{
  std::string tfilename=output_dir+"/toa";  
  
  FILE * pFile;
  
  pFile = fopen (tfilename.c_str(),"a");
  fprintf(pFile,"%-9s%19.13llf%6.2f%12.5f%9.6f%4s\n",itoa.psrname.c_str(),itoa.TOAMJD,itoa.TOAerr,itoa.freq,itoa.ddm,itoa.obscode.c_str());
  fclose (pFile);
  std::cout<<"МПИ добавлены в файл: "<<tfilename<<std::endl;
  return 1;
};
// Endof TOAWrite
//**********************************************************************************************


//**********************************************************************************************
// Запись результат кросс-корреляции в файл output_dir/kkf
int Cor::KKFWrite(std::string output_dir, Skkf kkf)
{ 
  std::string tfilename=output_dir+"/kkf";
  
  FILE * pFile;
  
  pFile = fopen (tfilename.c_str(),"a");
  fprintf (pFile, "%10.4f%10.4f%10.4f%10.4f%10.4f%15.2f%15.2f%15.2f \n", kkf.kk[0],kkf.kk[1],kkf.kk[2],kkf.kk[3],kkf.kk[4],kkf.maxp, kkf.errmax, kkf.snr);
  fclose (pFile);
  std::cout<<"Параметры кросс-корреляции добавлены в файл: "<<tfilename<<std::endl;
  return 1;
};
//Endof KKFWrite
//**********************************************************************************************


//**********************************************************************************************
// Запись значения ККФ в файл output_dir/*.kkf
int Cor::KKFdatWrite(std::string output_dir, Skkf kkf, std::string runs)
{
  
  std::ofstream kkfStream;
  char tmp[100];
  sprintf(tmp,"%s/%s.kkf",output_dir.c_str(),runs.c_str());
  kkfStream.open(tmp);
  
  for (int i=0; i<kkf.numpoint-1; i++){
    kkfStream<<i<<"     "<<kkf.kkfdata[i]<<"\n";
  }
  std::cout<<"ККФ записана в файл: "<<tmp<<std::endl;
  return 1;
}

//*********************************************************************************************
// расчет центра масс вектора данных

float Cor::CofM(std::vector<float> data, int datalength, float dt)
{
  float cm, mass;
  cm=0;
  mass=0;
  float min,max;
  
  min=0;
  min=max;
  
  //for (int i=0; i<datalength-1; i++)
  //	{
  //	if (max <= d[i]) max=d[i];
  //	if (min >= d[i]) min=d[i];
  //	}
  //for (int i=0; i<datalength-1; i++) d[i]=(d[i]-min)/(max-min); // вычитание 0 и нормировка на 1
  
  for (int i=0; i<datalength; i++) {mass=mass+data[i];};
  for (int i=0; i<datalength; i++) {cm=cm+data[i]*dt*i*1000.;};
  cm=cm/mass;
  return cm;
}


//**********************************************************************************************
//расчет дискретной ККФ кросс-корреляцией шаблона и профиля и норммирование на 1

std::vector<float> Cor::dccf(Tpl tpl, SumProfile prf)
  
{
  Cor cor;
  int ntpl = tpl.numpoint;         // число точек в шаблоне
  const int np = prf.numpointwin;   // число точек в профиле     
  std::vector<float> d;            // дискретная ккф
  const int kkflen = np;                     // длина массива ККФ по умолчанию если массив профиля > массива шаблона = длине профиля
  float min,max;
   std::vector<float> kkfarr=std::vector<float>(kkflen,0);            // массив для корреляции 
  float swap;
  std::vector<float> pr=std::vector<float>(np,0);                   // временный массив с профилем
  
  
  for (int i=0; i<kkflen; i++) kkfarr[i]=0; // инициализация массивов
  for (int i=0; i<np; i++) pr[i]=prf.prfdata[i]; 
  
  pr[np-1]=pr[np-2]; // устранение вероятного '0' в последней точке профиля
    
  for (int i=0; i<kkflen; i++)    // расчет ККФ 
    {          
      for (int j=0; j<ntpl; j++) kkfarr[i]=kkfarr[i]+tpl.tpldata[j]*pr[j]/ntpl; // расчет ккф
      swap=pr[0];        // циклический сдвиг массива влево
      for (int k=0; k<kkflen-1; k++) pr[k]=pr[k+1]; 
      pr[kkflen-1]=swap;
    };
  
  max=kkfarr[0];
  min=max;
  for (int i=0; i<kkflen; i++)
    {
      if (max <= kkfarr[i]) max=kkfarr[i];
      if (min >= kkfarr[i]) min=kkfarr[i];
    }
  for (int i=0; i<kkflen; i++) kkfarr[i]=(kkfarr[i]-min)/(max-min); // вычитание 0 и нормировка на 1
  for (int i=0; i<kkflen; i++) d.push_back(kkfarr[i]);              // возврат значения в вектор
  return d;
}; 





//**********************************************************************************************
//Расчет максимума ККФ вписыванием полинома 4 степени 5 точек вблизи максимума дискретной ККФ, 
// возвращает положение реального максимума относительно дискретного в отсчетах


/// проблема в  цикл перестановке в этой функции???

float Cor::ApproxMax(float y1, float y2, float y3, float y4, float y5) 
{
  
  //float y1,y2,y3,y4,y5; //y3 - точка максимума
  int npoli=4001; // число точек на единицу сетки в которыx ищется максимум путем перебора на сетке с дискретом 1/100 сетки
  float x,maxpoli,smaxpoli,ndelta,maxpoint;
  
  float a,b,c,d,e; // коэффициенты полинома 5 степени на сетке -2,-1,0,1,2
  
  //y1=kkfarr[maxpoint-2];y2=kkfarr[maxpoint-1];y3=kkfarr[maxpoint];y4=kkfarr[maxpoint+1];y5=kkfarr[maxpoint+2];
  //test
  //y1=-1.; y2=0.3; y3=1.; y4=1.; y5=-1.;
  
  a=y1/24-y2/6+y3/4-y4/6+y5/24;
  b=-(y1/12)+y2/6-y4/6+y5/12;
  c=-(y1/24)+(2*y2)/3-(5*y3)/4 + (2*y4)/3 - y5/24;
  d=1./12*(y1-8*y2+8*y4-y5);
  e=y3;
  
  //поиск локального максимума ККФ вблизи точки дискретного максимума перебором по сетке до 1/100 ККФ  
  
  // maxpoli0=kkfarr[maxpoint-2];
  maxpoli=y1;
  //std::cout << "x =   " << x << "ndelta =  " << ndelta << std::endl;
  for (int i=0; i<npoli; i++)
    {
      ndelta = 4./(npoli-1); 
      x=-2.+i*ndelta; // перебор по сетке от -2 до 2
      smaxpoli=a*x*x*x*x+b*x*x*x+c*x*x+d*x+e;
      if (smaxpoli>=maxpoli)  {maxpoli=smaxpoli; maxpoint=x;};
    };
  
  // printf ("MAXIMUM CCF########## из ApproxMax: %13f\n%10f \n", maxpoli,maxpoint);
  return maxpoint;
};
//**************************************************************************************************

//**************************************************************************************************
// расчет ошибки вписывания шаблона возвращает значение в мкс
// оценка ошибки вписывания по ширине и SNR элемента профиля, Труды ФИАН т.199 "Пульсары", ф.(11) стр. 153

float Cor::CalcErrorW50(SumProfile prf,float snr)
{
  int n=prf.numpointwin;
  int right,left;
  //float a[n];
  std::vector<float> a=std::vector<float>(n,0);
  float sigma;
  float sum;
  float max;
  int imax;
  float w50; // ширина импульса на 50% интенсивности
  
  max=0; imax=0; right=0; left=0;
  for (int i=0; i<n; i++) a[i]=prf.prfdata[i]; a[n-1]=a[n-2];// присвоение значений временного массива
  for (int i=0; i<n; i++) if (max <= a[i]) {max=a[i]; imax=i;};
  
  right=imax; while (a[right]>=max/2.) right++;
  left=imax; while (a[left]>=max/2.) left--;
  w50=prf.tau*(right-left)*1000;
  
  // std::cout << "right=" << right << "  left=" << left << "   w50=" << w50 << std::endl;
  
  
  return 0.3*w50/snr;
}; 

//**************************************************************************************************


//**************************************************************************************************
// расчет отношения сигнал/шум в профиле
float Cor::SNR(SumProfile prf)
{
  int n=prf.numpointwin;int nsigmapoints;
  int counter;
  float min;
  float max;
  std::vector<float> a=std::vector<float>(n,0);
  std::vector<float> sa=std::vector<float>(n,0);
  //float a[n],sa[n];
  float sigma;
  float sum;
  float average;
  float porog=0.1; // порог отсечки для вычисления СКО
  float step=0.05;
  
  for (int i=0; i<n; i++) a[i]=prf.prfdata[i]; a[n-1]=a[n-2];// присвоение значений временного массива
  
  min=0; max=0;
  // вычитание 0 и нормировка на 1
  for (int i=0; i<n; i++)
    {
      if (max <= a[i]) max=a[i];
      if (min >= a[i]) min=a[i];
    };
  for (int i=0; i<n; i++) a[i]=(a[i]-min)/(max-min); 
  for (int i=2; i<n-2; i++) sa[i]=(a[i-2]+a[i-1]+a[i]+a[i+1]+a[i+2])/5; sa[0]=sa[2]; sa[1]=sa[2]; sa[n-2]=sa[n-3]; sa[n-1]=sa[n-3];// сглаживание вспом массива по 5 точкам
  
  sum=0.; nsigmapoints=0; sigma=0; average=0;
  // расчет шумовой дорожки до 10% максимума несглаженного профиля, сглаживание введно для подавления импульсных помех при малых SNR
  counter=n-1;
  do 
    {	
      while (sa[counter]<=porog) {sum=sum+a[counter]; nsigmapoints++; counter--;}
      porog+=step;
    } while (nsigmapoints <=10); 
  
  //std::cout << ">> nsigmapoints, porog =   " << nsigmapoints <<" , " << porog-step<< std::endl;
  
  average=sum/nsigmapoints;
  for (int i=n-nsigmapoints; i<n; i++) sigma+=(a[i]-average)*(a[i]-average);
  sigma=sqrt(sigma/nsigmapoints); // вычисление СКО
  
  return 1./sigma; 
}; 

//**************************************************************************************************
