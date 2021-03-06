// ---------------------------------------------------------------------
//
// Copyright (c) 2014 - 2019 by the IBAMR developers
// All rights reserved.
//
// This file is part of IBAMR.
//
// IBAMR is free software and is distributed under the 3-clause BSD
// license. The full text of the license can be found in the file
// COPYRIGHT at the top level directory of IBAMR.
//
// ---------------------------------------------------------------------

/////////////////////////////// INCLUDES /////////////////////////////////////

#include "ibamr/IBAnchorPointSpec.h"
#include "ibamr/namespaces.h" // IWYU pragma: keep

#include "ibtk/StreamableFactory.h"
#include "ibtk/StreamableManager.h"

#include "tbox/SAMRAI_MPI.h"

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBAMR
{
/////////////////////////////// STATIC ///////////////////////////////////////

int IBAnchorPointSpec::STREAMABLE_CLASS_ID = StreamableManager::getUnregisteredID();

void
IBAnchorPointSpec::registerWithStreamableManager()
{
    // We place MPI barriers here to ensure that all MPI processes actually
    // register the factory class with the StreamableManager, and to ensure that
    // all processes employ the same class ID for the IBAnchorPointSpec object.
    SAMRAI_MPI::barrier();
    if (!getIsRegisteredWithStreamableManager())
    {
#if !defined(NDEBUG)
        TBOX_ASSERT(STREAMABLE_CLASS_ID == StreamableManager::getUnregisteredID());
#endif
        STREAMABLE_CLASS_ID = StreamableManager::getManager()->registerFactory(new IBAnchorPointSpecFactory());
    }
    SAMRAI_MPI::barrier();
    return;
} // registerWithStreamableManager

/////////////////////////////// PUBLIC ///////////////////////////////////////

/////////////////////////////// PROTECTED ////////////////////////////////////

/////////////////////////////// PRIVATE //////////////////////////////////////

/////////////////////////////// NAMESPACE ////////////////////////////////////

} // namespace IBAMR

//////////////////////////////////////////////////////////////////////////////
