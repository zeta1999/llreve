/*
 * This file is part of
 *    llreve - Automatic regression verification for LLVM programs
 *
 * Copyright (C) 2016 Karlsruhe Institute of Technology
 *
 * The system is published under a BSD license.
 * See LICENSE (distributed with this file) for details.
 */

#pragma once

#include "llvm/IR/Module.h"
#include "core/Criterion.h"
#include "core/SliceCandidate.h"
#include "smtSolver/SmtSolver.h"

#include "MonoPair.h"
#include "Opts.h"
#include "Preprocess.h"

enum class ValidationResult {valid, invalid, unknown};

extern bool CriterionPresent;

class SliceCandidateValidation {
public:
	static ValidationResult validate(llvm::Module* program, llvm::Module* candidate,
		CriterionPtr criterion = Criterion::getReturnValueCriterion(),
		CEXType* pCEX = nullptr);
	static ValidationResult validate(SliceCandidate* candidate, CEXType* pCEX = nullptr);

	static void activateHeap();
private:
	static bool heap;
	static std::vector<MonoPair<PreprocessedFunction>> preprocessCandidate(SliceCandidatePtr candidate,
                     PreprocessOpts opts);
};
