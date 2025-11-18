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

void UMssSessionDataWidget::SetSessionInfo(const FOnlineSessionSearchResult& InSessionSearchResultRef, const FString& InMapName, const FString& InPlayers, const FString& InGameMode)
{
	SessionSearchResultRef = InSessionSearchResultRef;
	
	MapName->SetText(FText::FromString(InMapName));
	Players->SetText(FText::FromString(InPlayers));
	GameMode->SetText(FText::FromString(InGameMode));
}

void UMssSessionDataWidget::SetMssHUDRef(UMssHUD* InMssHUD)
{
	MssHUDRef = InMssHUD;
}
