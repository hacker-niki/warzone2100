#include <memory>
#include "lib/widget/widgbase.h"
#include "lib/widget/button.h"
#include "lib/widget/bar.h"
#include "build_interface.h"
#include "../objmem.h"
#include "../hci.h"
#include "../research.h"
#include "../statsdef.h"
#include "../order.h"
#include "../intorder.h"
#include "../mission.h"
#include "../qtscript.h"
#include "../display3d.h"
#include "../warcam.h"

#define STAT_GAP			2
#define STAT_BUTWIDTH		60
#define STAT_BUTHEIGHT		46

static void addBuildObjectStats(std::shared_ptr<BuildInterfaceController> controller);

void BuildInterfaceController::updateBuildersList()
{
	std::vector<DROID *> newBuilders;
	for (DROID *droid = apsDroidLists[selectedPlayer]; droid; droid = droid->psNext)
	{
		if ((droid->droidType == DROID_CONSTRUCT || droid->droidType == DROID_CYBORG_CONSTRUCT) && droid->died == 0)
		{
			newBuilders.push_back(droid);
		}
	}

	objects = std::vector<BASE_OBJECT *>(newBuilders.rbegin(), newBuilders.rend());
}

void BuildInterfaceController::updateBuildOptionsList()
{
	std::vector<STRUCTURE_STATS *> newBuildOptions;

	bool researchModule;
	bool factoryModule;
	bool powerModule;

	//check to see if able to build research/factory modules
	researchModule = factoryModule = powerModule = false;

	//if currently on a mission can't build factory/research/power/derricks
	if (!missionIsOffworld())
	{
		for (auto psCurr = apsStructLists[selectedPlayer]; psCurr != nullptr; psCurr = psCurr->psNext)
		{
			if (psCurr->pStructureType->type == REF_RESEARCH && psCurr->status == SS_BUILT)
			{
				researchModule = true;
			}
			else if (psCurr->pStructureType->type == REF_FACTORY && psCurr->status == SS_BUILT)
			{
				factoryModule = true;
			}
			else if (psCurr->pStructureType->type == REF_POWER_GEN && psCurr->status == SS_BUILT)
			{
				powerModule = true;
			}
		}
	}

	//set the list of Structures to build
	for (auto inc = 0; inc < numStructureStats; inc++)
	{
		//if the structure is flagged as available, add it to the list
		if (apStructTypeLists[selectedPlayer][inc] != AVAILABLE && (!includeRedundantDesigns || apStructTypeLists[selectedPlayer][inc] != REDUNDANT))
		{
			continue;
		}

		auto &structureStat = asStructureStats[inc];

		//check not built the maximum allowed already
		if (structureStat.curCount[selectedPlayer] < structureStat.upgrade[selectedPlayer].limit)
		{
			//don't want corner wall to appear in list
			if (structureStat.type == REF_WALLCORNER)
			{
				continue;
			}

			// Remove the demolish stat from the list for tutorial
			// tjc 4-dec-98  ...
			if (bInTutorial && structureStat.type == REF_DEMOLISH)
			{
				continue;
			}

			//can't build list when offworld
			if (missionIsOffworld())
			{
				if (structureStat.type == REF_FACTORY ||
					structureStat.type == REF_POWER_GEN ||
					structureStat.type == REF_RESOURCE_EXTRACTOR ||
					structureStat.type == REF_RESEARCH ||
					structureStat.type == REF_CYBORG_FACTORY ||
					structureStat.type == REF_VTOL_FACTORY)
				{
					continue;
				}
			}

			if (structureStat.type == REF_RESEARCH_MODULE)
			{
				//don't add to list if Research Facility not presently built
				if (!researchModule)
				{
					continue;
				}
			}
			else if (structureStat.type == REF_FACTORY_MODULE)
			{
				//don't add to list if Factory not presently built
				if (!factoryModule)
				{
					continue;
				}
			}
			else if (structureStat.type == REF_POWER_MODULE)
			{
				//don't add to list if Power Gen not presently built
				if (!powerModule)
				{
					continue;
				}
			}
			// show only favorites?
			if (shouldShowFavorites() && !asStructureStats[inc].isFavorite)
			{
				continue;
			}

			debug(LOG_NEVER, "adding %s (%x)", getStatsName(&structureStat), apStructTypeLists[selectedPlayer][inc]);
			newBuildOptions.push_back(&structureStat);
		}
	}

	stats = std::vector<BASE_STATS *>(newBuildOptions.begin(), newBuildOptions.end());
}

BASE_STATS *BuildInterfaceController::getObjectStatsAt(size_t objectIndex) const
{
	auto builder = castDroid(getObjectAt(objectIndex));
	if (!builder)
	{
		return nullptr;
	}

	if (!(droidType(builder) == DROID_CONSTRUCT || droidType(builder) == DROID_CYBORG_CONSTRUCT))
	{
		return nullptr;
	}

	BASE_STATS *stats;
	if (orderStateStatsLoc(builder, DORDER_BUILD, &stats)) // Moving to build location?
	{
		return stats;
	}

	if (builder->order.type == DORDER_BUILD && orderStateObj(builder, DORDER_BUILD)) // Is building
	{
		return builder->order.psStats;
	}

	if (builder->order.type == DORDER_HELPBUILD || builder->order.type == DORDER_LINEBUILD) // Is helping
	{
		if (auto structure = orderStateObj(builder, DORDER_HELPBUILD))
		{
			return ((STRUCTURE *)structure)->pStructureType;
		}
	}

	if (orderState(builder, DORDER_DEMOLISH))
	{
		return structGetDemolishStat();
	}

	return nullptr;
}


void BuildInterfaceController::startBuildPosition(BASE_STATS *buildOption)
{
	ASSERT_OR_RETURN(, castDroid(getSelectedObject()) != nullptr, "invalid droid pointer");
	
	triggerEvent(TRIGGER_MENU_BUILD_SELECTED);

	//check for demolish first
	if (buildOption == structGetDemolishStat())
	{
		objMode = IOBJ_DEMOLISHSEL;
	}
	else
	{
		intSetPositionStats(buildOption);

		/* Now start looking for a location for the structure */
		objMode = IOBJ_BUILDSEL;

		intStartStructPosition(buildOption);
	}

	// Close the stats box
	intRemoveStats();
	intMode = INT_OBJECT;

	closeInterface();
}

void BuildInterfaceController::selectBuilder(BASE_OBJECT *builder)
{
	clearSelection();
	builder->selected = true;
	triggerEventSelected();
	jsDebugSelected(builder);
	setSelectedObject(builder);
}

void BuildInterfaceController::jumpToSelectedBuilder()
{
	auto builder = getSelectedObject();
	setPlayerPos(builder->pos.x, builder->pos.y);
	if (getWarCamStatus())
	{
		camToggleStatus();
	}
}

void BuildInterfaceController::addToFavorites(BASE_STATS *buildOption)
{
	asStructureStats[buildOption->index].isFavorite = !shouldShowFavorites();
}

void BuildInterfaceController::refresh()
{
	updateBuildersList();
	updateBuildOptionsList();

	if (objectsSize() == 0)
	{
		closeInterface();
		return;
	}

	auto selected = getSelectedObject();
	if (!selected || selected->died)
	{
		findSelected();
	}
}

void BuildInterfaceController::closeInterface()
{
	intRemoveStats();
	intRemoveObject();
}

void BuildInterfaceController::findSelected()
{
	auto selectedBuilder = getSelectedObject();
	for (auto builder: objects)
	{
		if (builder->died == 0 && builder == selectedBuilder)
		{
			return;
		}
	}

	selectedBuilder = nullptr;
	for (auto builder: objects)
	{
		if (builder->died == 0 && builder->selected)
		{
			selectedBuilder = builder;
			break;
		}
	}

	if (!selectedBuilder)
	{
		selectedBuilder = objects.front();
	}

	setSelectedObject(selectedBuilder);
}

class BuildObjectButton : public IntFancyButton
{
private:
	typedef IntFancyButton BaseWidget;
public:
	BuildObjectButton(const std::shared_ptr<BaseObjectsStatsController> &controller, size_t builderIndex)
		: BaseWidget()
		, controller(controller)
		, builderIndex(builderIndex)
	{
		buttonType = BTMBUTTON;
	}

	void display(int xOffset, int yOffset) override
	{
		initDisplay();

		auto droid = castDroid(controller->getObjectAt(builderIndex));
		if (droid && !isDead(droid))
		{
			displayIMD(Image(), ImdObject::Droid(droid), xOffset, yOffset);
		}

		displayIfHighlight(xOffset, yOffset);
		doneDisplay();
	}

	void clicked(W_CONTEXT *context, WIDGET_KEY mouseButton = WKEY_PRIMARY) override
	{
		BaseWidget::clicked(context, mouseButton);
		auto droid = controller->getObjectAt(builderIndex);

		if (droid && mouseButton == WKEY_PRIMARY)
		{
			auto buildController = std::static_pointer_cast<BuildInterfaceController>(controller);
			buildController->selectBuilder(droid);
			buildController->jumpToSelectedBuilder();
		}
	}

protected:
	std::shared_ptr<BaseObjectsStatsController> controller;
	size_t builderIndex;
};

class BuildStatsButton: public ObjectStatsButton
{
private:
	typedef ObjectStatsButton BaseWidget;
public:
	using BaseWidget::BaseWidget;

protected:
	bool isSelected() const override
	{
		auto droid = controller->getObjectAt(buttonIndex);
		return droid && (droid->selected || droid == controller->getSelectedObject());
	}

	void clicked(W_CONTEXT *context, WIDGET_KEY mouseButton = WKEY_PRIMARY) override
	{
		BaseWidget::clicked(context, mouseButton);
		auto droid = controller->getObjectAt(buttonIndex);

		if (droid && mouseButton == WKEY_PRIMARY)
		{
			std::static_pointer_cast<BuildInterfaceController>(controller)->selectBuilder(droid);
		}
	}
};

class BuildOptionButton: public StatsFormButton
{
private:
	typedef StatsFormButton BaseWidget;

	BuildOptionButton(): BaseWidget() {}
public:
	static std::shared_ptr<BuildOptionButton> make(const std::shared_ptr<BuildInterfaceController> &controller, size_t buildOptionIndex)
	{
		class make_shared_enabler: public BuildOptionButton {};
		auto widget = std::make_shared<make_shared_enabler>();
		widget->controller = controller;
		widget->buildOptionIndex = buildOptionIndex;
		widget->initialize();
		return widget;
	}

protected:
	BASE_STATS *getStats() override
	{
		return controller->getStatsAt(buildOptionIndex);
	}

	void initialize()
	{
		W_BARINIT sBarInit;
		sBarInit.id = IDSTAT_TIMEBARSTART;
		sBarInit.x = STAT_TIMEBARX;
		sBarInit.y = STAT_TIMEBARY;
		sBarInit.width = STAT_PROGBARWIDTH;
		sBarInit.height = STAT_PROGBARHEIGHT;
		sBarInit.size = 50;
		sBarInit.sCol = WZCOL_ACTION_PROGRESS_BAR_MAJOR;
		sBarInit.sMinorCol = WZCOL_ACTION_PROGRESS_BAR_MINOR;
		if (sBarInit.size > 100)
		{
			sBarInit.size = 100;
		}

		sBarInit.iRange = GAME_TICKS_PER_SEC;
		attach(bar = std::make_shared<W_BARGRAPH>(&sBarInit));
		bar->setBackgroundColour(WZCOL_BLACK);
	}

	void display(int xOffset, int yOffset) override
	{
		updateLayout();
		BaseWidget::display(xOffset, yOffset);
	}

	bool isSelected() const override
	{
		if (auto form = statsForm.lock())
		{
			return form->getSelectedStats() == controller->getStatsAt(buildOptionIndex);
		}

		return false;
	}

	void updateLayout()
	{
		auto stat = getStats();
		auto powerCost = ((STRUCTURE_STATS *)stat)->powerToBuild;
		bar->majorSize = powerCost / POWERPOINTS_DROIDDIV;
		WzString costString = WzString::fromUtf8(_("\nCost: %1"));
		costString.replace("%1", WzString::number(powerCost));
		WzString tipString = getStatsName(stat);
		tipString.append(costString);
		setTip(tipString.toUtf8().c_str());
	}

	void clicked(W_CONTEXT *context, WIDGET_KEY mouseButton = WKEY_PRIMARY) override
	{
		BaseWidget::clicked(context, mouseButton);
		auto clickedStats = controller->getStatsAt(buildOptionIndex);
		ASSERT_OR_RETURN(, clickedStats != nullptr, "Invalid template pointer");

		if (mouseButton == WKEY_PRIMARY)
		{
			controller->startBuildPosition(clickedStats);
		}
		else if (mouseButton == WKEY_SECONDARY)
		{
			controller->addToFavorites(clickedStats);
		}
	}

	std::shared_ptr<BuildInterfaceController> controller;
	std::shared_ptr<W_BARGRAPH> bar;
	size_t buildOptionIndex;
};

class BuildObjectsForm: public ObjectsForm
{
private:
	typedef ObjectsForm BaseWidget;
protected:
	using BaseWidget::BaseWidget;
public:
	static std::shared_ptr<BuildObjectsForm> make(const std::shared_ptr<BaseObjectsStatsController> &controller)
	{
		class make_shared_enabler: public BuildObjectsForm
		{
		public:
			make_shared_enabler(const std::shared_ptr<BaseObjectsStatsController> &controller): BuildObjectsForm(controller)
			{
			}
		};
		auto widget = std::make_shared<make_shared_enabler>(controller);
		widget->initialize();
		return widget;
	}

	std::shared_ptr<ObjectStatsButton> makeStatsButton(size_t buttonIndex) const override
		{
		return std::make_shared<BuildStatsButton>(controller, buttonIndex);
	}

	std::shared_ptr<IntFancyButton> makeObjectButton(size_t buttonIndex) const override
	{
		return std::make_shared<BuildObjectButton>(controller, buttonIndex);
	}
};

class BuildStatsForm: public StatsForm
{
private:
	typedef StatsForm BaseWidget;
protected:
	using BaseWidget::BaseWidget;
public:
	static std::shared_ptr<BuildStatsForm> make(const std::shared_ptr<BuildInterfaceController> &controller)
	{
		class make_shared_enabler: public BuildStatsForm
		{
		public:
			make_shared_enabler(const std::shared_ptr<BuildInterfaceController> &controller): BuildStatsForm(controller)
			{
			}
		};
		auto widget = std::make_shared<make_shared_enabler>(controller);
		widget->buildController = controller;
		widget->initialize();
		return widget;
	}

	std::shared_ptr<StatsFormButton> makeOptionButton(size_t buttonIndex) const override
	{
		return BuildOptionButton::make(buildController, buttonIndex);
	}

protected:
	void initialize() override
	{
		BaseWidget::initialize();
		addObsoleteButton();
		addFavoriteButton();
	}

	void addObsoleteButton()
	{
		attach(obsoleteButton = std::make_shared<MultipleChoiceButton>());
		obsoleteButton->style |= WBUT_SECONDARY;
		obsoleteButton->setChoice(buildController->shouldShowRedundantDesign());
		obsoleteButton->setImages(false, MultipleChoiceButton::Images(Image(IntImages, IMAGE_OBSOLETE_HIDE_UP), Image(IntImages, IMAGE_OBSOLETE_HIDE_UP), Image(IntImages, IMAGE_OBSOLETE_HIDE_HI)));
		obsoleteButton->setTip(false, _("Hiding Obsolete Tech"));
		obsoleteButton->setImages(true,  MultipleChoiceButton::Images(Image(IntImages, IMAGE_OBSOLETE_SHOW_UP), Image(IntImages, IMAGE_OBSOLETE_SHOW_UP), Image(IntImages, IMAGE_OBSOLETE_SHOW_HI)));
		obsoleteButton->setTip(true, _("Showing Obsolete Tech"));
		obsoleteButton->move(4 + Image(IntImages, IMAGE_FDP_UP).width() + 4, STAT_SLDY);

		auto weakController = std::weak_ptr<BuildInterfaceController>(buildController);
		obsoleteButton->addOnClickHandler([weakController](W_BUTTON &button) {
			if (auto buildController = weakController.lock())
			{
				auto &obsoleteButton = static_cast<MultipleChoiceButton &>(button);
				auto newValue = !obsoleteButton.getChoice();
				buildController->setShouldShowRedundantDesign(newValue);
				obsoleteButton.setChoice(newValue);
			}
		});
	}

	void addFavoriteButton()
	{
		attach(favoriteButton = std::make_shared<MultipleChoiceButton>());
		favoriteButton->style |= WBUT_SECONDARY;
		favoriteButton->setChoice(buildController->shouldShowFavorites());
		favoriteButton->setImages(false, MultipleChoiceButton::Images(Image(IntImages, IMAGE_ALLY_RESEARCH), Image(IntImages, IMAGE_ALLY_RESEARCH), Image(IntImages, IMAGE_ALLY_RESEARCH)));
		favoriteButton->setTip(false, _("Showing All Tech\nRight-click to add to Favorites"));
		favoriteButton->setImages(true,  MultipleChoiceButton::Images(Image(IntImages, IMAGE_ALLY_RESEARCH_TC), Image(IntImages, IMAGE_ALLY_RESEARCH_TC), Image(IntImages, IMAGE_ALLY_RESEARCH_TC)));
		favoriteButton->setTip(true, _("Showing Only Favorite Tech\nRight-click to remove from Favorites"));
		favoriteButton->move(4 * 2 + Image(IntImages, IMAGE_FDP_UP).width() * 2 + 4 * 2, STAT_SLDY);

		auto weakController = std::weak_ptr<BuildInterfaceController>(buildController);
		favoriteButton->addOnClickHandler([weakController](W_BUTTON &button) {
			if (auto buildController = weakController.lock())
			{
				auto &favoriteButton = static_cast<MultipleChoiceButton &>(button);
				auto newValue = !favoriteButton.getChoice();
				buildController->setShouldShowFavorite(newValue);
				favoriteButton.setChoice(newValue);
			}
		});
	}

	std::shared_ptr<BuildInterfaceController> buildController;
	std::shared_ptr<MultipleChoiceButton> obsoleteButton;
	std::shared_ptr<MultipleChoiceButton> favoriteButton;
};

bool BuildInterfaceController::showInterface()
{
	intRemoveStatsNoAnim();
	intRemoveOrderNoAnim();

	if (objects.empty())
	{
		return false;
	}

	findSelected();

	auto const &parent = psWScreen->psForm;
	auto objectsForm = BuildObjectsForm::make(shared_from_this());
	parent->attach(objectsForm);

	addBuildObjectStats(shared_from_this());

	intMode = INT_STAT;
	triggerEvent(TRIGGER_MENU_BUILD_UP);

	return true;
}

static void addBuildObjectStats(std::shared_ptr<BuildInterfaceController> controller)
{

	auto const &parent = psWScreen->psForm;

	auto statForm = BuildStatsForm::make(controller);
	parent->attach(statForm);
}
