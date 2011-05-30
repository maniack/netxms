/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2010 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.console;

import org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer;
import org.eclipse.jface.preference.IPreferenceStore;

/**
 * Preference initializer for console
 *
 */
public class PreferenceInitializer extends AbstractPreferenceInitializer
{
	/* (non-Javadoc)
	 * @see org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer#initializeDefaultPreferences()
	 */
	@Override
	public void initializeDefaultPreferences()
	{
		final IPreferenceStore ps = Activator.getDefault().getPreferenceStore();
		ps.setDefault("INITIAL_PERSPECTIVE", "org.netxms.ui.eclipse.console.DefaultPerspective");
		ps.setDefault("SHOW_COOLBAR", true);
		ps.setDefault("SHOW_TRAY_ICON", true);
		ps.setDefault("HIDE_WHEN_MINIMIZED", false);
		ps.setDefault("SAVE_AND_RESTORE", true);
		ps.setDefault("HTTP_PROXY_ENABLED", false);
		ps.setDefault("HTTP_PROXY_SERVER", "");
		ps.setDefault("HTTP_PROXY_PORT", "8000");
		ps.setDefault("HTTP_PROXY_EXCLUSIONS", "localhost|127.0.0.1|10.*|192.168.*");
	}
}
