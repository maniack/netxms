/**
 * 
 */
package org.netxms.ui.android.main.fragments;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.Interface;
import org.netxms.ui.android.R;
import org.netxms.ui.android.loaders.GenericObjectChildrenLoader;
import org.netxms.ui.android.main.adapters.InterfacesAdapter;

import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * Fragment for last values info
 * 
 * @author Marco Incalcaterra (marco.incalcaterra@thinksoft.it)
 * 
 */

public class InterfacesFragment extends ExpandableListFragment implements LoaderManager.LoaderCallbacks<Set<AbstractObject>>
{
	private InterfacesAdapter adapter = null;
	private final GenericObjectChildrenLoader loader = null;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState)
	{
		View v = inflater.inflate(R.layout.interfaces_fragment, container, false);
		createProgress(v);
		return v;
	}

	@Override
	public void onActivityCreated(Bundle savedInstanceState)
	{
		super.onActivityCreated(savedInstanceState);
		adapter = new InterfacesAdapter(getActivity(), null, null);
		setListAdapter(adapter);
		setListShown(false, true);
		GenericObjectChildrenLoader loader = (GenericObjectChildrenLoader)getActivity().getSupportLoaderManager().initLoader(R.layout.interfaces_fragment, null, this);
		if (loader != null)
		{
			loader.setObjId(nodeId);
			loader.setClassFilter(AbstractObject.OBJECT_INTERFACE);
			loader.setService(service);
		}
	}

	@Override
	public void refresh()
	{
		if (loader != null)
		{
			loader.setObjId(nodeId);
			loader.setService(service);
			loader.forceLoad();
		}
	}

	/* (non-Javadoc)
	 * @see android.support.v4.app.LoaderManager.LoaderCallbacks#onCreateLoader(int, android.os.Bundle)
	 */
	@Override
	public Loader<Set<AbstractObject>> onCreateLoader(int arg0, Bundle arg1)
	{
		return new GenericObjectChildrenLoader(getActivity());
	}

	/* (non-Javadoc)
	 * @see android.support.v4.app.LoaderManager.LoaderCallbacks#onLoadFinished(android.support.v4.content.Loader, java.lang.Object)
	 */
	@Override
	public void onLoadFinished(Loader<Set<AbstractObject>> arg0, Set<AbstractObject> arg1)
	{
		setListShown(true, true);
		if (adapter != null)
		{
			List<Interface> interfaces = null;
			if (arg1 != null)
			{
				interfaces = new ArrayList<Interface>();
				for (AbstractObject go : arg1)
					interfaces.add((Interface)go);
			}
			adapter.setValues(interfaces);
			adapter.notifyDataSetChanged();
		}
	}
	/* (non-Javadoc)
	 * @see android.support.v4.app.LoaderManager.LoaderCallbacks#onLoaderReset(android.support.v4.content.Loader)
	 */
	@Override
	public void onLoaderReset(Loader<Set<AbstractObject>> arg0)
	{
	}
}
