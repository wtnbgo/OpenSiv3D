//-----------------------------------------------
//
//	This file is part of the Siv3D Engine.
//
//	Copyright (c) 2008-2025 Ryo Suzuki
//	Copyright (c) 2016-2025 OpenSiv3D Project
//
//	Licensed under the MIT License.
//
//-----------------------------------------------

# include <Siv3D/Monitor.hpp>
# include <Siv3D/MonitorInfo.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/Common/OpenGL.hpp>

namespace s3d
{
	namespace System
	{
		Array<MonitorInfo> EnumerateMonitors()
		{
			Array<MonitorInfo> results;
			
			int numDisplays = 0;
			SDL_DisplayID* displays = SDL_GetDisplays(&numDisplays);
			
			if (!displays)
			{
				return results;
			}

			const SDL_DisplayID primaryDisplayID = SDL_GetPrimaryDisplay();

			for (int i = 0; i < numDisplays; ++i)
			{
				SDL_DisplayID displayID = displays[i];
				
				// Name
				const String name = Unicode::Widen(SDL_GetDisplayName(displayID));
				
				// Bounds
				SDL_Rect bounds;
				SDL_GetDisplayBounds(displayID, &bounds);
				
				// WorkArea
				SDL_Rect workArea;
				SDL_GetDisplayUsableBounds(displayID, &workArea);
				
				// Physical Size
				// SDL3 does not provide physical size directly in mm.
				Optional<Size> sizeMillimeter;
				
				// Scaling
				Optional<double> scaling;
				{
					float scale = SDL_GetDisplayContentScale(displayID);
					if (scale > 0.0f)
					{
						scaling = scale;
					}
				}
				
				// Refresh Rate & Fullscreen Resolution
				Optional<double> refreshRate;
				Size fullscreenResolution{ 0, 0 };
				
				const SDL_DisplayMode* currentMode = SDL_GetCurrentDisplayMode(displayID);
				if (currentMode)
				{
					if (currentMode->refresh_rate > 0)
					{
						refreshRate = currentMode->refresh_rate;
					}
				}
				
				// Get max resolution
				int numModes = 0;
				SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &numModes);
				if (modes && numModes > 0)
				{
					int maxW = 0, maxH = 0;
					for(int m=0; m<numModes; ++m)
					{
						if (modes[m]->w > maxW)
						{
							maxW = modes[m]->w;
							maxH = modes[m]->h;
						}
						else if (modes[m]->w == maxW && modes[m]->h > maxH)
						{
							maxH = modes[m]->h;
						}
					}
					fullscreenResolution.set(maxW, maxH);
					SDL_free(modes);
				}
				else if (currentMode)
				{
					fullscreenResolution.set(currentMode->w, currentMode->h);
				}

				const MonitorInfo info =
				{
					.name					= name,
					.id						= Format(displayID),
					.displayDeviceName		= name,
					.displayRect			= Rect(bounds.x, bounds.y, bounds.w, bounds.h),
					.workArea				= Rect(workArea.x, workArea.y, workArea.w, workArea.h),
					.fullscreenResolution	= fullscreenResolution,
					.isPrimary				= (displayID == primaryDisplayID),
					.sizeMillimeter			= sizeMillimeter,
					.scaling				= scaling,
					.refreshRate			= refreshRate,
				};
				
				results << info;
			}
			
			SDL_free(displays);
			
			return results;
		}
	}
}
