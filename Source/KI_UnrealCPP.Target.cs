// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class KI_UnrealCPPTarget : TargetRules
{
	public KI_UnrealCPPTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

        DefaultBuildSettings = BuildSettingsVersion.V6;        
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;


		ExtraModuleNames.AddRange( new string[] { "KI_UnrealCPP" } );
	}
}
