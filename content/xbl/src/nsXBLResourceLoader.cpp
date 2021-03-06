/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "nsString.h"
#include "nsCSSStyleSheet.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"
#include "nsXBLResourceLoader.h"
#include "nsXBLPrototypeResources.h"
#include "nsIDocumentObserver.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "mozilla/css/Loader.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsGkAtoms.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsXBLPrototypeBinding.h"
#include "nsCSSRuleProcessor.h"
#include "nsContentUtils.h"
#include "nsStyleSet.h"
#include "nsIScriptSecurityManager.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXBLResourceLoader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXBLResourceLoader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mBoundElements)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXBLResourceLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mBoundElements)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLResourceLoader)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXBLResourceLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXBLResourceLoader)

nsXBLResourceLoader::nsXBLResourceLoader(nsXBLPrototypeBinding* aBinding,
                                         nsXBLPrototypeResources* aResources)
:mBinding(aBinding),
 mResources(aResources),
 mResourceList(nsnull),
 mLastResource(nsnull),
 mLoadingResources(false),
 mInLoadResourcesFunc(false),
 mPendingSheets(0)
{
}

nsXBLResourceLoader::~nsXBLResourceLoader()
{
  delete mResourceList;
}

void
nsXBLResourceLoader::LoadResources(bool* aResult)
{
  mInLoadResourcesFunc = true;

  if (mLoadingResources) {
    *aResult = (mPendingSheets == 0);
    mInLoadResourcesFunc = false;
    return;
  }

  mLoadingResources = true;
  *aResult = true;

  // Declare our loaders.
  nsCOMPtr<nsIDocument> doc = mBinding->XBLDocumentInfo()->GetDocument();

  mozilla::css::Loader* cssLoader = doc->CSSLoader();
  nsIURI *docURL = doc->GetDocumentURI();
  nsIPrincipal* docPrincipal = doc->NodePrincipal();

  nsCOMPtr<nsIURI> url;

  for (nsXBLResource* curr = mResourceList; curr; curr = curr->mNext) {
    if (curr->mSrc.IsEmpty())
      continue;

    if (NS_FAILED(NS_NewURI(getter_AddRefs(url), curr->mSrc,
                            doc->GetDocumentCharacterSet().get(), docURL)))
      continue;

    if (curr->mType == nsGkAtoms::image) {
      if (!nsContentUtils::CanLoadImage(url, doc, doc, docPrincipal)) {
        // We're not permitted to load this image, move on...
        continue;
      }

      // Now kick off the image load...
      // Passing NULL for pretty much everything -- cause we don't care!
      // XXX: initialDocumentURI is NULL! 
      nsCOMPtr<imgIRequest> req;
      nsContentUtils::LoadImage(url, doc, docPrincipal, docURL, nsnull,
                                nsIRequest::LOAD_BACKGROUND,
                                getter_AddRefs(req));
    }
    else if (curr->mType == nsGkAtoms::stylesheet) {
      // Kick off the load of the stylesheet.

      // Always load chrome synchronously
      // XXXbz should that still do a content policy check?
      bool chrome;
      nsresult rv;
      if (NS_SUCCEEDED(url->SchemeIs("chrome", &chrome)) && chrome)
      {
        rv = nsContentUtils::GetSecurityManager()->
          CheckLoadURIWithPrincipal(docPrincipal, url,
                                    nsIScriptSecurityManager::ALLOW_CHROME);
        if (NS_SUCCEEDED(rv)) {
          nsRefPtr<nsCSSStyleSheet> sheet;
          rv = cssLoader->LoadSheetSync(url, getter_AddRefs(sheet));
          NS_ASSERTION(NS_SUCCEEDED(rv), "Load failed!!!");
          if (NS_SUCCEEDED(rv))
          {
            rv = StyleSheetLoaded(sheet, false, NS_OK);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Processing the style sheet failed!!!");
          }
        }
      }
      else
      {
        rv = cssLoader->LoadSheet(url, docPrincipal, EmptyCString(), this);
        if (NS_SUCCEEDED(rv))
          ++mPendingSheets;
      }
    }
  }

  *aResult = (mPendingSheets == 0);
  mInLoadResourcesFunc = false;
  
  // Destroy our resource list.
  delete mResourceList;
  mResourceList = nsnull;
}

// nsICSSLoaderObserver
NS_IMETHODIMP
nsXBLResourceLoader::StyleSheetLoaded(nsCSSStyleSheet* aSheet,
                                      bool aWasAlternate,
                                      nsresult aStatus)
{
  if (!mResources) {
    // Our resources got destroyed -- just bail out
    return NS_OK;
  }
   
  mResources->mStyleSheetList.AppendElement(aSheet);

  if (!mInLoadResourcesFunc)
    mPendingSheets--;
  
  if (mPendingSheets == 0) {
    // All stylesheets are loaded.  
    mResources->mRuleProcessor =
      new nsCSSRuleProcessor(mResources->mStyleSheetList, 
                             nsStyleSet::eDocSheet);

    // XXX Check for mPendingScripts when scripts also come online.
    if (!mInLoadResourcesFunc)
      NotifyBoundElements();
  }
  return NS_OK;
}

void 
nsXBLResourceLoader::AddResource(nsIAtom* aResourceType, const nsAString& aSrc)
{
  nsXBLResource* res = new nsXBLResource(aResourceType, aSrc);
  if (!res)
    return;

  if (!mResourceList)
    mResourceList = res;
  else
    mLastResource->mNext = res;

  mLastResource = res;
}

void
nsXBLResourceLoader::AddResourceListener(nsIContent* aBoundElement) 
{
  if (aBoundElement) {
    mBoundElements.AppendObject(aBoundElement);
  }
}

void
nsXBLResourceLoader::NotifyBoundElements()
{
  nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1"));
  nsIURI* bindingURI = mBinding->BindingURI();

  PRUint32 eltCount = mBoundElements.Count();
  for (PRUint32 j = 0; j < eltCount; j++) {
    nsCOMPtr<nsIContent> content = mBoundElements.ObjectAt(j);
    
    bool ready = false;
    xblService->BindingReady(content, bindingURI, &ready);

    if (ready) {
      // We need the document to flush out frame construction and
      // such, so we want to use the current document.
      nsIDocument* doc = content->GetCurrentDoc();
    
      if (doc) {
        // Flush first to make sure we can get the frame for content
        doc->FlushPendingNotifications(Flush_Frames);

        // If |content| is (in addition to having binding |mBinding|)
        // also a descendant of another element with binding |mBinding|,
        // then we might have just constructed it due to the
        // notification of its parent.  (We can know about both if the
        // binding loads were triggered from the DOM rather than frame
        // construction.)  So we have to check both whether the element
        // has a primary frame and whether it's in the undisplayed map
        // before sending a ContentInserted notification, or bad things
        // will happen.
        nsIPresShell *shell = doc->GetShell();
        if (shell) {
          nsIFrame* childFrame = content->GetPrimaryFrame();
          if (!childFrame) {
            // Check to see if it's in the undisplayed content map.
            nsStyleContext* sc =
              shell->FrameManager()->GetUndisplayedContent(content);

            if (!sc) {
              shell->RecreateFramesFor(content);
            }
          }
        }

        // Flush again
        // XXXbz why is this needed?
        doc->FlushPendingNotifications(Flush_ContentAndNotify);
      }
    }
  }

  // Clear out the whole array.
  mBoundElements.Clear();

  // Delete ourselves.
  NS_RELEASE(mResources->mLoader);
}

nsresult
nsXBLResourceLoader::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv;

  for (nsXBLResource* curr = mResourceList; curr; curr = curr->mNext) {
    if (curr->mType == nsGkAtoms::image)
      rv = aStream->Write8(XBLBinding_Serialize_Image);
    else if (curr->mType == nsGkAtoms::stylesheet)
      rv = aStream->Write8(XBLBinding_Serialize_Stylesheet);
    else
      continue;

    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(curr->mSrc.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
