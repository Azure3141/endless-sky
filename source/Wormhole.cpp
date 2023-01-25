/* Wormhole.cpp
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "Wormhole.h"

#include "DataNode.h"
#include "GameData.h"
#include "Planet.h"
#include "System.h"

#include <algorithm>

using namespace std;



// Default constructor.
Wormhole::Wormhole()
{
	linkColor = ExclusiveItem<Color>(GameData::Colors().Get("map wormhole"));
}



// Load a wormhole's description from a file.
void Wormhole::Load(const DataNode &node)
{
	if(node.Size() < 2)
		return;
	isDefined = true;
	isAutogenerated = false;

	bool seenLinkAttribute = false;
	for(const DataNode &child : node)
	{
		// Check for the "add" or "remove" keyword.
		bool add = (child.Token(0) == "add");
		bool remove = (child.Token(0) == "remove");
		if((add || remove) && child.Size() < 2)
		{
			child.PrintTrace("Skipping " + child.Token(0) + " with no key given:");
			continue;
		}

		// Get the key and value (if any).
		const string &key = child.Token((add || remove) ? 1 : 0);
		int valueIndex = (add || remove) ? 2 : 1;
		bool hasValue = (child.Size() > valueIndex);
		const string &value = child.Token(hasValue ? valueIndex : 0);

		// Check for conditions that require clearing this key's current value.
		// "remove <key>" means to clear the key's previous contents.
		// "remove <key> <value>" means to remove just that value from the key.
		if((remove && !hasValue) && key == "link")
		{
			links.clear();
			continue;
		}

		// "link" attributes are always cleared for new data definitions
		// that introduce new links, except when explicitly adding.
		if(key == "link" && !seenLinkAttribute && !add)
		{
			links.clear();
			seenLinkAttribute = true;
		}

		// Handle the attributes which can be "removed."
		if(key == "link" && child.Size() > valueIndex + 1)
		{
			const auto *from = GameData::Systems().Get(child.Token(valueIndex));
			const auto *to = GameData::Systems().Get(child.Token(valueIndex + 1));
			if(remove)
			{
				// Only erase if the link is an exact match.
				auto it = links.find(from);
				if(it != links.end() && it->second == to)
					links.erase(from);
				else
					child.PrintTrace("Unable to remove non-existent link:");
			}
			else
				links[from] = to;
		}
		else if(key == "mappable")
			mappable = !remove;
		else if(key == "display name")
		{
			if(remove)
				name = "???";
			else if(hasValue)
				name = value;
			else
				child.PrintTrace("Missing value for attribute:");
		}
		else if(key == "color" && hasValue)
		{
			if(child.Size() >= 3 + valueIndex)
				linkColor = ExclusiveItem<Color>(Color(child.Value(valueIndex),
						child.Value(valueIndex + 1), child.Value(valueIndex + 2)));
			else if(child.Size() >= 1 + valueIndex)
				linkColor = ExclusiveItem<Color>(GameData::Colors().Get(child.Token(valueIndex)));
			else
				child.PrintTrace("Warning: skipping malformed "color" node:");
		}
		else if(remove)
			child.PrintTrace("Cannot \"remove\" a specific value from the given key:");
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



void Wormhole::LoadFromPlanet(const Planet &planet)
{
	this->planet = &planet;
	mappable = !planet.Description().empty();
	GenerateLinks();
	isAutogenerated = true;
	isDefined = true;
}



bool Wormhole::IsValid() const
{
	if(!isDefined)
		return false;
	if(!planet || !planet->IsValid())
		return false;

	for(auto &&pair : links)
		if(!pair.first->IsValid() || !pair.second->IsValid())
			return false;

	return true;
}



const Planet *Wormhole::GetPlanet() const
{
	return planet;
}



const string &Wormhole::Name() const
{
	return name;
}



bool Wormhole::IsMappable() const
{
	return mappable;
}



const Color *Wormhole::GetLinkColor() const
{
	return &(*linkColor);
}



bool Wormhole::IsAutogenerated() const
{
	return isAutogenerated;
}



const System &Wormhole::WormholeSource(const System &to) const
{
	using value_type = decltype(links)::value_type;
	auto it = find_if(links.begin(), links.end(),
			[&to](const value_type &val)
			{
				return val.second == &to;
			});

	return it == links.end() ? to : *it->first;
}




const System &Wormhole::WormholeDestination(const System &from) const
{
	auto it = links.find(&from);
	return it == links.end() ? from : *it->second;
}



const unordered_map<const System *, const System *> &Wormhole::Links() const
{
	return links;
}



void Wormhole::SetPlanet(const Planet &planet)
{
	this->planet = &planet;
}



void Wormhole::GenerateLinks()
{
	// Clear any previous links since we're regenerating every link.
	links.clear();

	// Wormhole links form a closed loop through every system the planet is in.
	for(size_t i = 0; i < planet->Systems().size(); ++i)
	{
		int next = i == planet->Systems().size() - 1 ? 0 : i + 1;
		// But check whether the wormhole in the given system has a sprite.
		// If not, this is a one way wormhole that shouldn't be linked.
		if(planet->Systems()[i]->FindStellar(planet)->GetSprite())
			links[planet->Systems()[i]] = planet->Systems()[next];
	}
}
