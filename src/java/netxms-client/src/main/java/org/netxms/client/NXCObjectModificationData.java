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
package org.netxms.client;

import java.util.Map;

/**
 * @author Victor
 * 
 * This class is used to hold data for NXCSession.modifyObject()
 *
 */
public class NXCObjectModificationData
{
	// Modification flags
	public static final long MODIFY_NAME              = 0x00000001L;
	public static final long MODIFY_ACL               = 0x00000002L;
	public static final long MODIFY_CUSTOM_ATTRIBUTES = 0x00000004L;
	public static final long MODIFY_AUTO_APPLY        = 0x00000008L;
	public static final long MODIFY_AUTO_BIND         = 0x00000010L;
	public static final long MODIFY_POLICY_CONFIG     = 0x00000020L;
	public static final long MODIFY_VERSION           = 0x00000040L;
	public static final long MODIFY_DESCRIPTION       = 0x00000080L;
	public static final long MODIFY_AGENT_PORT        = 0x00000100L;
	public static final long MODIFY_AGENT_AUTH        = 0x00000200L;
	public static final long MODIFY_SNMP_VERSION      = 0x00000400L;
	public static final long MODIFY_SNMP_AUTH         = 0x00000800L;
	public static final long MODIFY_AGENT_PROXY       = 0x00001000L;
	public static final long MODIFY_SNMP_PROXY        = 0x00002000L;
	public static final long MODIFY_TRUSTED_NODES     = 0x00004000L;
	public static final long MODIFY_GEOLOCATION       = 0x00008000L;
	
	private long flags;		// Flags which indicates what object's data should be modified
	private long objectId;
	private String name;
	private NXCAccessListElement[] acl;
	private boolean inheritAccessRights;
	private Map<String, String> customAttributes;
	private boolean autoApplyEnabled;
	private String autoApplyFilter;
	private boolean autoBindEnabled;
	private String autoBindFilter;
	private String configFileName;
	private String configFileContent;
	private int version;
	private String description;
	private int agentPort;
	private int agentAuthMethod;
	private String agentSecret;
	private long agentProxy;
	private int snmpVersion;
	private int snmpAuthMethod;
	private int snmpPrivMethod;
	private String snmpAuthName;
	private String snmpAuthPassword;
	private String snmpPrivPassword;
	private long snmpProxy;
	private long[] trustedNodes;
	private GeoLocation geolocation;
	
	/**
	 * Constructor for creating modification data for given object
	 */
	public NXCObjectModificationData(long objectId)
	{
		this.objectId = objectId;
		flags = 0;
	}

	/**
	 * @return the objectId
	 */
	public long getObjectId()
	{
		return objectId;
	}

	/**
	 * @param objectId the objectId to set
	 */
	public void setObjectId(long objectId)
	{
		this.objectId = objectId;
	}

	/**
	 * @return the name
	 */
	public String getName()
	{
		return name;
	}

	/**
	 * @param name the name to set
	 */
	public void setName(final String name)
	{
		this.name = name;
		flags |= MODIFY_NAME;
	}

	/**
	 * @return the flags
	 */
	public long getFlags()
	{
		return flags;
	}

	/**
	 * @return the acl
	 */
	public NXCAccessListElement[] getACL()
	{
		return (acl != null) ? acl : new NXCAccessListElement[0];
	}

	/**
	 * @param acl the acl to set
	 */
	public void setACL(NXCAccessListElement[] acl)
	{
		this.acl = acl;
		flags |= MODIFY_ACL;
	}

	/**
	 * @return the inheritAccessRights
	 */
	public boolean isInheritAccessRights()
	{
		return inheritAccessRights;
	}

	/**
	 * @param inheritAccessRights the inheritAccessRights to set
	 */
	public void setInheritAccessRights(boolean inheritAccessRights)
	{
		this.inheritAccessRights = inheritAccessRights;
		flags |= MODIFY_ACL;
	}

	/**
	 * @return the customAttributes
	 */
	public Map<String, String> getCustomAttributes()
	{
		return customAttributes;
	}

	/**
	 * @param customAttributes the customAttributes to set
	 */
	public void setCustomAttributes(Map<String, String> customAttributes)
	{
		this.customAttributes = customAttributes;
		flags |= MODIFY_CUSTOM_ATTRIBUTES;
	}

	/**
	 * @return the autoApplyEnabled
	 */
	public boolean isAutoApplyEnabled()
	{
		return autoApplyEnabled;
	}

	/**
	 * @param autoApplyEnabled the autoApplyEnabled to set
	 */
	public void setAutoApplyEnabled(boolean autoApplyEnabled)
	{
		this.autoApplyEnabled = autoApplyEnabled;
		flags |= MODIFY_AUTO_APPLY;
	}

	/**
	 * @return the autoApplyFilter
	 */
	public String getAutoApplyFilter()
	{
		return autoApplyFilter;
	}

	/**
	 * @param autoApplyFilter the autoApplyFilter to set
	 */
	public void setAutoApplyFilter(String autoApplyFilter)
	{
		this.autoApplyFilter = autoApplyFilter;
		flags |= MODIFY_AUTO_APPLY;
	}

	/**
	 * @return the autoApplyEnabled
	 */
	public boolean isAutoBindEnabled()
	{
		return autoBindEnabled;
	}

	/**
	 * @param autoApplyEnabled the autoApplyEnabled to set
	 */
	public void setAutoBindEnabled(boolean autoBindEnabled)
	{
		this.autoBindEnabled = autoBindEnabled;
		flags |= MODIFY_AUTO_BIND;
	}

	/**
	 * @return the autoApplyFilter
	 */
	public String getAutoBindFilter()
	{
		return autoBindFilter;
	}

	/**
	 * @param autoApplyFilter the autoApplyFilter to set
	 */
	public void setAutoBindFilter(String autoBindFilter)
	{
		this.autoBindFilter = autoBindFilter;
		flags |= MODIFY_AUTO_BIND;
	}

	/**
	 * @return the configFileName
	 */
	public String getConfigFileName()
	{
		return configFileName;
	}

	/**
	 * @param configFileName the configFileName to set
	 */
	public void setConfigFileName(String configFileName)
	{
		this.configFileName = configFileName;
		flags |= MODIFY_POLICY_CONFIG;
	}

	/**
	 * @return the configFileContent
	 */
	public String getConfigFileContent()
	{
		return configFileContent;
	}

	/**
	 * @param configFileContent the configFileContent to set
	 */
	public void setConfigFileContent(String configFileContent)
	{
		this.configFileContent = configFileContent;
		flags |= MODIFY_POLICY_CONFIG;
	}

	/**
	 * @return the version
	 */
	public int getVersion()
	{
		return version;
	}

	/**
	 * @param version the version to set
	 */
	public void setVersion(int version)
	{
		this.version = version;
		flags |= MODIFY_VERSION;
	}

	/**
	 * @return the description
	 */
	public String getDescription()
	{
		return description;
	}

	/**
	 * @param description the description to set
	 */
	public void setDescription(String description)
	{
		this.description = description;
		flags |= MODIFY_DESCRIPTION;
	}

	public int getAgentPort()
	{
		return agentPort;
	}

	public void setAgentPort(int agentPort)
	{
		this.agentPort = agentPort;
		flags |= MODIFY_AGENT_PORT;
	}

	/**
	 * @return the agentAuthMethod
	 */
	public int getAgentAuthMethod()
	{
		return agentAuthMethod;
	}

	/**
	 * @param agentAuthMethod the agentAuthMethod to set
	 */
	public void setAgentAuthMethod(int agentAuthMethod)
	{
		this.agentAuthMethod = agentAuthMethod;
		flags |= MODIFY_AGENT_AUTH;
	}

	/**
	 * @return the agentSecret
	 */
	public String getAgentSecret()
	{
		return agentSecret;
	}

	/**
	 * @param agentSecret the agentSecret to set
	 */
	public void setAgentSecret(String agentSecret)
	{
		this.agentSecret = agentSecret;
		flags |= MODIFY_AGENT_AUTH;
	}

	/**
	 * @return the agentProxy
	 */
	public long getAgentProxy()
	{
		return agentProxy;
	}

	/**
	 * @param agentProxy the agentProxy to set
	 */
	public void setAgentProxy(long agentProxy)
	{
		this.agentProxy = agentProxy;
		flags |= MODIFY_AGENT_PROXY;
	}

	/**
	 * @return the snmpVersion
	 */
	public int getSnmpVersion()
	{
		return snmpVersion;
	}

	/**
	 * @param snmpVersion the snmpVersion to set
	 */
	public void setSnmpVersion(int snmpVersion)
	{
		this.snmpVersion = snmpVersion;
		flags |= MODIFY_SNMP_VERSION;
	}

	/**
	 * @return the snmpAuthMethod
	 */
	public int getSnmpAuthMethod()
	{
		return snmpAuthMethod;
	}

	/**
	 * @param snmpAuthMethod the snmpAuthMethod to set
	 */
	public void setSnmpAuthMethod(int snmpAuthMethod)
	{
		this.snmpAuthMethod = snmpAuthMethod;
		flags |= MODIFY_SNMP_AUTH;
	}

	/**
	 * @return the snmpPrivMethod
	 */
	public int getSnmpPrivMethod()
	{
		return snmpPrivMethod;
	}

	/**
	 * @param snmpPrivMethod the snmpPrivMethod to set
	 */
	public void setSnmpPrivMethod(int snmpPrivMethod)
	{
		this.snmpPrivMethod = snmpPrivMethod;
		flags |= MODIFY_SNMP_AUTH;
	}

	/**
	 * @return the snmpAuthName
	 */
	public String getSnmpAuthName()
	{
		return snmpAuthName;
	}

	/**
	 * @param snmpAuthName the snmpAuthName to set
	 */
	public void setSnmpAuthName(String snmpAuthName)
	{
		this.snmpAuthName = snmpAuthName;
		flags |= MODIFY_SNMP_AUTH;
	}

	/**
	 * @return the snmpAuthPassword
	 */
	public String getSnmpAuthPassword()
	{
		return snmpAuthPassword;
	}

	/**
	 * @param snmpAuthPassword the snmpAuthPassword to set
	 */
	public void setSnmpAuthPassword(String snmpAuthPassword)
	{
		this.snmpAuthPassword = snmpAuthPassword;
		flags |= MODIFY_SNMP_AUTH;
	}

	/**
	 * @return the snmpPrivPassword
	 */
	public String getSnmpPrivPassword()
	{
		return snmpPrivPassword;
	}

	/**
	 * @param snmpPrivPassword the snmpPrivPassword to set
	 */
	public void setSnmpPrivPassword(String snmpPrivPassword)
	{
		this.snmpPrivPassword = snmpPrivPassword;
		flags |= MODIFY_SNMP_AUTH;
	}

	/**
	 * @return the snmpProxy
	 */
	public long getSnmpProxy()
	{
		return snmpProxy;
	}

	/**
	 * @param snmpProxy the snmpProxy to set
	 */
	public void setSnmpProxy(long snmpProxy)
	{
		this.snmpProxy = snmpProxy;
		flags |= MODIFY_SNMP_PROXY;
	}

	/**
	 * @return the trustedNodes
	 */
	public long[] getTrustedNodes()
	{
		return trustedNodes;
	}

	/**
	 * @param trustedNodes the trustedNodes to set
	 */
	public void setTrustedNodes(long[] trustedNodes)
	{
		this.trustedNodes = trustedNodes;
		flags |= MODIFY_TRUSTED_NODES;
	}

	/**
	 * @return the geolocation
	 */
	public GeoLocation getGeolocation()
	{
		return geolocation;
	}

	/**
	 * @param geolocation the geolocation to set
	 */
	public void setGeolocation(GeoLocation geolocation)
	{
		this.geolocation = geolocation;
		flags |= MODIFY_GEOLOCATION;
	}
}
