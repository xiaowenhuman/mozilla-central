/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnection.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

#define VOICECHANGE_EVENTNAME      NS_LITERAL_STRING("voicechange")
#define DATACHANGE_EVENTNAME       NS_LITERAL_STRING("datachange")
#define CARDSTATECHANGE_EVENTNAME  NS_LITERAL_STRING("cardstatechange")

DOMCI_DATA(MozMobileConnection, mozilla::dom::network::MobileConnection)

namespace mozilla {
namespace dom {
namespace network {

const char* kVoiceChangedTopic     = "mobile-connection-voice-changed";
const char* kDataChangedTopic      = "mobile-connection-data-changed";
const char* kCardStateChangedTopic = "mobile-connection-cardstate-changed";

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(cardstatechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(voicechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(datachange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(cardstatechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(voicechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(datachange)
  tmp->mProvider = nsnull;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MobileConnection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMobileConnection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozMobileConnection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMobileConnection)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileConnection, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileConnection, nsDOMEventTargetHelper)

MobileConnection::MobileConnection()
{
  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIMobileConnectionProvider!");
  }
}

void
MobileConnection::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not acquire nsIObserverService!");
    return;
  }

  obs->AddObserver(this, kVoiceChangedTopic, false);
  obs->AddObserver(this, kDataChangedTopic, false);
  obs->AddObserver(this, kCardStateChangedTopic, false);
}

void
MobileConnection::Shutdown()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not acquire nsIObserverService!");
    return;
  }

  obs->RemoveObserver(this, kVoiceChangedTopic);
  obs->RemoveObserver(this, kDataChangedTopic);
  obs->RemoveObserver(this, kCardStateChangedTopic);
}

// nsIObserver

NS_IMETHODIMP
MobileConnection::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const PRUnichar* aData)
{
  if (!strcmp(aTopic, kVoiceChangedTopic)) {
    InternalDispatchEvent(VOICECHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kDataChangedTopic)) {
    InternalDispatchEvent(DATACHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kCardStateChangedTopic)) {
    InternalDispatchEvent(CARDSTATECHANGE_EVENTNAME);
    return NS_OK;
  }

  MOZ_NOT_REACHED("Unknown observer topic!");
  return NS_OK;
}

// nsIDOMMozMobileConnection

NS_IMETHODIMP
MobileConnection::GetCardState(nsAString& cardState)
{
  if (!mProvider) {
    cardState.SetIsVoid(true);
    return NS_OK;
  }
  return mProvider->GetCardState(cardState);
}

NS_IMETHODIMP
MobileConnection::GetVoice(nsIDOMMozMobileConnectionInfo** voice)
{
  if (!mProvider) {
    *voice = nsnull;
    return NS_OK;
  }
  return mProvider->GetVoiceConnectionInfo(voice);
}

NS_IMETHODIMP
MobileConnection::GetData(nsIDOMMozMobileConnectionInfo** data)
{
  if (!mProvider) {
    *data = nsnull;
    return NS_OK;
  }
  return mProvider->GetDataConnectionInfo(data);
}

NS_IMETHODIMP
MobileConnection::GetNetworks(nsIDOMDOMRequest** request)
{
  *request = nsnull;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetNetworks(GetOwner(), request);
}

nsresult
MobileConnection::InternalDispatchEvent(const nsAString& aType)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(aType, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(MobileConnection, cardstatechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, voicechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, datachange)

} // namespace network
} // namespace dom
} // namespace mozilla
