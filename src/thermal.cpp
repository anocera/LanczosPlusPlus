#include "Vector.h"
#include "IoSimple.h"
#include "Matrix.h"
#include "BLAS.h"
#include "Tokenizer.h"

typedef double RealType;
typedef PsimagLite::Matrix<RealType> MatrixType;
typedef PsimagLite::Vector<RealType>::Type VectorRealType;
typedef PsimagLite::Vector<SizeType>::Type VectorSizeType;
typedef PsimagLite::IoSimple::In InputType;

class OneSector {

public:

	OneSector(InputType& io)
	{
		io.read(sector_,"#SectorSource");
		io.read(eigs_,"#Eigenvalues");
		io.readMatrix(vecs_,"#Eigenvectors");
	}

	bool isSector(const VectorSizeType& jndVector) const
	{
		return (jndVector == sector_);
	}

	void info(std::ostream& os) const
	{
		os<<"sector\n";
		os<<sector_;
		os<<"eigs.size()="<<eigs_.size()<<"\n";
		os<<"vecs="<<vecs_.n_row()<<"x"<<vecs_.n_col()<<"\n";
	}

	SizeType size() const { return eigs_.size(); }

	void multiplyRight(MatrixType& x,const MatrixType& a) const
	{
		SizeType n = a.n_row();
		SizeType m = a.n_col();
		assert(vecs_.n_row() == m);
		assert(vecs_.n_col() == m);
		assert(x.n_row() == n);
		assert(x.n_col() == m);
		assert(m > 0 && n > 0);
		psimag::BLAS::GEMM('N','N',n,m,m,1.0,&(a(0,0)),n,&(vecs_(0,0)),m,0.0,&(x(0,0)),n);
	}

	void multiplyLeft(MatrixType& x,const MatrixType& a) const
	{
		SizeType n = a.n_row();
		SizeType m = a.n_col();
		assert(vecs_.n_row() == n);
		psimag::BLAS::GEMM('C','N',n,m,n,1.0,&(vecs_(0,0)),n,&(a(0,0)),n,0.0,&(x(0,0)),n);
	}

	const RealType& eig(SizeType i) const
	{
		assert(i < eigs_.size());
		return eigs_[i];
	}

private:

	VectorSizeType sector_;
	VectorRealType eigs_;
	MatrixType vecs_;
};

typedef PsimagLite::Vector<OneSector*>::Type VectorOneSectorType;

struct ThermalOptions {
	ThermalOptions(PsimagLite::String operatorName_,
	               RealType beta_,
	               const VectorSizeType& sites_)
	    : operatorName(operatorName_),
	      beta(beta_),
	      sites(sites_)
	{}

	PsimagLite::String operatorName;
	RealType beta;
	VectorSizeType sites;
};

//Compute X^(s,s')_{n,n'} = \sum_{t,t'}U^{s*}_{n,t}A_{t,t'}^(s,s')U^{s'}_{t',n'}
void computeX(MatrixType& x,
              const MatrixType& a,
              const OneSector& sectorSrc,
              const OneSector& sectorDest)
{
	MatrixType tmp(x.n_row(),x.n_col());
	sectorDest.multiplyRight(tmp,a);
	sectorSrc.multiplyLeft(x,tmp);
}

SizeType findJnd(const VectorOneSectorType& sectors, const VectorSizeType& jndVector)
{
	for (SizeType i = 0; i < sectors.size(); ++i) {
		if (sectors[i]->isSector(jndVector)) return i;
	}

	PsimagLite::String str(__FILE__);
	str += " " + ttos(__LINE__) + "\n";
	str += "findJnd can't find sector index\n";
	throw PsimagLite::RuntimeError(str);
}

void findOperatorAndMatrix(MatrixType& a,
                           SizeType& jnd,
                           SizeType siteIndex,
                           SizeType ind,
                           const ThermalOptions& opt,
                           const VectorOneSectorType& sectors,
                           InputType& io)
{
	if (opt.operatorName == "i") {
		jnd = ind;
		SizeType n = sectors[ind]->size();
		a.resize(n,n);
		a.setTo(0);
		for (SizeType i = 0; i < n; ++i)
			a(i,i) = 1.0;

		return;
	} else if (opt.operatorName != "c") {
		PsimagLite::String str(__FILE__);
		str += " " + ttos(__LINE__) + "\n";
		str += "findOperatorAndMatrix: unknown operator " + opt.operatorName + "\n";
		throw PsimagLite::RuntimeError(str);
	}

	SizeType spin = 0;
	assert(opt.sites.size() > siteIndex);
	SizeType site = opt.sites[siteIndex];
	PsimagLite::String label = "#Operator_c_";
	label += ttos(spin) + "_" + ttos(site);
	io.advance(label,0);
	VectorSizeType jndVector;
	io.read(jndVector,"#SectorDest");
	if (jndVector.size() == 0) return;
	jnd = findJnd(sectors,jndVector);

	io.readMatrix(a,"#Matrix");
}

RealType computeThisSector(SizeType ind,
                           const ThermalOptions& opt,
                           const VectorOneSectorType& sectors,
                           InputType& io)
{
	SizeType jnd = 0;
	MatrixType a;
	findOperatorAndMatrix(a,jnd,0,ind,opt,sectors,io);

	SizeType n = a.n_row();
	if (n == 0) return 0.0;
	assert(n == sectors[ind]->size());

	SizeType m = a.n_col();
	assert(m == sectors[jnd]->size());
	if (m == 0) return 0.0;

	//Read operator 2   --> B
	MatrixType b;
	assert(opt.sites.size() == 2);
	if (opt.sites[0] == opt.sites[1]) {
		b = a;
	} else {
		assert(opt.sites[0] < opt.sites[1]);
		SizeType knd = 0;
		findOperatorAndMatrix(b,knd,1,ind,opt,sectors,io);

		if (jnd != knd) {
			PsimagLite::String str(__FILE__);
			str += " " + ttos(__LINE__) + "\n";
			str += "computeThisSector: too many destination sectors\n";
			throw PsimagLite::RuntimeError(str);
		}
	}

	//Compute X^(s,s')_{n,n'} = \sum_{t,t'}U^{s*}_{n,t}A_{t,t'}^(s,s')U^{s'}_{t',n'}
	MatrixType x(n,m);
	computeX(x,a,*(sectors[ind]),*(sectors[jnd]));
	// Same for Y from B
	MatrixType y(n,m);
	computeX(y,b,*(sectors[ind]),*(sectors[jnd]));
	// Result is
	// \sum_{n,n'} X_{n,n'} Y_{n',n} exp(-i(E_n'-E_n)t))exp(-beta E_n)
	RealType sum = 0.0;
	for (SizeType i = 0; i < n; ++i) {
		for (SizeType j = 0; j < m; ++j) {
			RealType e1 = sectors[ind]->eig(i);
			RealType e2 = sectors[jnd]->eig(j);
			RealType val = x(i,j)*std::conj(b(i,j))* exp(-opt.beta*e1);
			std::cout<<(e1-e2)<<" "<<val<<"\n";
			sum += val;
		}
	}

	return sum;
}

void computeAverageFor(const ThermalOptions& opt,
                       const VectorOneSectorType& sectors,
                       InputType& io)
{
	RealType sum = 0.0;
	for (SizeType i = 0; i < sectors.size(); ++i)
		sum += computeThisSector(i,opt,sectors,io);

	std::cerr<<"operator="<<opt.operatorName;
	std::cerr<<" beta="<<opt.beta<<" sum="<<sum<<"\n";
}

void usage(char *name, PsimagLite::String msg = "")
{
	if (msg != "") std::cerr<<name<<": "<<msg<<"\n";
	std::cerr<<"USAGE: "<<name<<" -f file -c operator -b beta -s site1[,site2]\n";
}

int main(int argc, char**argv)
{
	int opt = 0;
	PsimagLite::String operatorName;
	PsimagLite::String file;
	PsimagLite::Vector<PsimagLite::String>::Type tokens;
	VectorSizeType sites(2,0);
	RealType beta = 0;

	while ((opt = getopt(argc, argv, "f:c:b:s:")) != -1) {
		switch (opt) {
		case 'c':
			operatorName = optarg;
			break;
		case 'f':
			file = optarg;
			break;
		case 'b':
			beta = atof(optarg);
			break;
		case 's':
			PsimagLite::tokenizer(optarg,tokens,",");
			break;
		default: /* '?' */
			usage(argv[0]);
			return 1;
		}
	}

	if (file == "" || operatorName == "") {
		usage(argv[0]);
		return 2;
	}

	if (tokens.size() > sites.size()) {
		usage(argv[0],"Too many sites");
		return 3;
	}

	for (SizeType i = 0; i < tokens.size(); ++i)
		sites[i] = atoi(tokens[i].c_str());

	if (sites[1] < sites[0]) {
		usage(argv[0],"site1 must be smaller than site2");
	}

	InputType io(file);
	SizeType total = 0;
	io.readline(total,"#TotalSectors=");
	VectorOneSectorType sectors(total);

	for (SizeType i = 0; i < sectors.size(); ++i) {
		sectors[i] = new OneSector(io);
		//sectors[i]->info(std::cout);
	}

	ThermalOptions options(operatorName,beta,sites);
	io.rewind();
	computeAverageFor(options,sectors,io);

	for (SizeType i = 0; i < sectors.size(); ++i)
		delete sectors[i];
}
