// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Widgets/MssSessionDataWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
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
	if (MssHUDRef.IsValid())
		MssHUDRef->JoinTheGivenSession(SessionSearchResultRef);
	else
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSessionDataWidget::OnJoinSessionButtonClicked MssHUDRef is null"));
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
