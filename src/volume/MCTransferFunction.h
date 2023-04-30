#pragma once
#include "TransferFunction.h"

class MCTransferFunction {
	public:
		MCTransferFunction(std::string fileName);

		// transfer functions
		ColorTransferFunction1D  diffuseTF;
		ColorTransferFunction1D  specularTF;
		ColorTransferFunction1D  emissionTF;
		ScalarTransferFunction1D roughnessTF;
		ScalarTransferFunction1D opacityTF;
};