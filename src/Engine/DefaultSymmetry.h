
/*
// BEGIN LICENSE BLOCK
Copyright (c) 2009 , UT-Battelle, LLC
All rights reserved

[Lanczos++, Version 1.0.0]

*********************************************************
THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. 

Please see full open source license included in file LICENSE.
*********************************************************

*/

#ifndef DEFAULT_SYMM_H
#define DEFAULT_SYMM_H
#include <iostream>
#include "ProgressIndicator.h"
#include "CrsMatrix.h"
#include "Vector.h"
#include "Matrix.h"

namespace LanczosPlusPlus {


	template<typename GeometryType,typename BasisType>
	class DefaultSymmetry  {

		typedef typename GeometryType::ComplexOrRealType ComplexOrRealType;
		typedef typename PsimagLite::Real<ComplexOrRealType>::Type RealType;
		typedef typename BasisType::WordType WordType;

	public:

		typedef PsimagLite::CrsMatrix<RealType> SparseMatrixType;
		typedef typename PsimagLite::Vector<RealType>::Type VectorType;

		DefaultSymmetry(const BasisType& basis,
		                const GeometryType& geometry,
		                bool printMatrix)
		: printMatrix_(printMatrix)
		{}

		template<typename SomeModelType>
		void init(const SomeModelType& model,const BasisType& basis)
		{
			model.setupHamiltonian(matrixStored_,basis);
			bool nrows = matrixStored_.row();
			if (printMatrix_) {
				if (nrows > 40)
					throw PsimagLite::RuntimeError("printMatrix too big\n");
				std::cout<<matrixStored_.toDense();
				PsimagLite::Matrix<ComplexOrRealType> matrixCopy = matrixStored_.toDense();
				fullDiag(matrixCopy);
			}
		}

		void transformMatrix(typename PsimagLite::Vector<SparseMatrixType>::Type& matrix1,
		                     const SparseMatrixType& matrix) const
		{
			throw std::runtime_error("DefaultSymmetry: cannot call transformMatrix\n");
		}

		void transformGs(VectorType& gs,SizeType offset)
		{
		}

		SizeType sectors() const { return 1; }

		void setPointer(SizeType p) { }

		PsimagLite::String name() const { return "default"; }

		SizeType rank() const { return matrixStored_.row(); }

		template<typename SomeVectorType>
		void matrixVectorProduct(SomeVectorType &x, SomeVectorType const &y) const
		{
			return matrixStored_.matrixVectorProduct(x,y);
		}

	private:

		//! For debugging purpose only:
		void fullDiag(PsimagLite::Matrix<ComplexOrRealType>& fm) const
		{
			typename PsimagLite::Vector<RealType>::Type e(fm.n_row());
			diag(fm,e,'N');
			for (SizeType i=0;i<e.size();i++)
				std::cout<<e[i]<<"\n";
		}

		SparseMatrixType matrixStored_;
		bool printMatrix_;
	}; // class DefaultSymmetry
} // namespace Dmrg

#endif  // DEFAULT_SYMM_H
