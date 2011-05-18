/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2011 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.dashboard.propertypages.helpers;

import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Image;
import org.netxms.client.dashboards.DashboardElement;
import org.netxms.ui.eclipse.dashboard.propertypages.DashboardElements;

/**
 * Label provider for list of dashboard elements
 *
 */
public class DashboardElementsLabelProvider extends LabelProvider implements ITableLabelProvider
{
	private static final String[] ELEMENT_TYPES = { "Label", "Line Chart", "Bar Chart", "Pie Chart", "Tube Chart", "Status Chart", "Status Indicator", "Dashboard", "Network Map" };
	private static final String[] H_ALIGH = { "FILL", "CENTER", "LEFT", "RIGHT" };
	private static final String[] V_ALIGH = { "FILL", "CENTER", "TOP", "BOTTOM" };
	
	/* (non-Javadoc)
	 * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnImage(java.lang.Object, int)
	 */
	@Override
	public Image getColumnImage(Object element, int columnIndex)
	{
		return null;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnText(java.lang.Object, int)
	 */
	@Override
	public String getColumnText(Object element, int columnIndex)
	{
		DashboardElement de = (DashboardElement)element;
		switch(columnIndex)
		{
			case DashboardElements.COLUMN_TYPE:
				try
				{
					return ELEMENT_TYPES[de.getType()];
				}
				catch(ArrayIndexOutOfBoundsException e)
				{
					return "<unknown>";
				}
			case DashboardElements.COLUMN_SPAN:
				return Integer.toString(de.getHorizontalSpan()) + " / " + Integer.toString(de.getVerticalSpan());
			case DashboardElements.COLUMN_ALIGNMENT:
				try
				{
					return H_ALIGH[de.getHorizontalAlignment()] + " / " + V_ALIGH[de.getVerticalAlignment()];
				}
				catch(ArrayIndexOutOfBoundsException e)
				{
					return "<unknown>";
				}
		}
		return null;
	}
}
