// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Widgets/MssSessionDataWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "System/MssLogger.h"
#include "Widgets/MssHUD.h"

bool UMssSessionDataWidget::Initialize()
{
	if (!Super::Initialize())
		return false;

	if (JoinSessionButton)
		JoinSessionButton->OnClicked.AddDynamic(this, &ThisClass::OnJoinSessionButtonClicked);
	
	return true;
}

void UMssSessionDataWidget::OnJoinSessionButtonClicked()
{
	if (!MssHUDRef.IsValid())
	{
		LOG_ERROR(TEXT("MssHUDRef is INVALID"));
		return;
	}
	
	MssHUDRef->JoinTheGivenSession(SessionSearchResultRef);
}

void UMssSessionDataWidget::SetSessionInfo(const FOnlineSessionSearchResult& InSessionSearchResultRef, 
	const FTempCustomSessionSettings& SessionSettings)
{
	SessionSearchResultRef = InSessionSearchResultRef;
	
	MapName->SetText(FText::FromString(SessionSettings.MapName));
	Players->SetText(FText::FromString(SessionSettings.Players));
	GameMode->SetText(FText::FromString(SessionSettings.GameMode));
}

void UMssSessionDataWidget::SetMssHUDRef(UMssHUD* InMssHUD)
{
	MssHUDRef = InMssHUD;
}
