
/*
    Copyright David Nolden  <david.nolden.kdevelop@art-master.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef WORKINGSETCONTROLLER_H
#define WORKINGSETCONTROLLER_H

#include <QObject>
#include <QMap>
#include "uicontroller.h"
#include <sublime/area.h>
#include <qlabel.h>
#include <qtoolbutton.h>
#include <kicon.h>
#include "core.h"
#include <util/pushvalue.h>

class QHBoxLayout;

class KConfigGroup;

namespace Sublime {
class Area;
class AreaIndex;
}

namespace KDevelop {
class UiController;
class MainWindow;
class Core;

class WorkingSet : public QObject {
    Q_OBJECT
public:
    WorkingSet(QString id, QString icon) ;
    
    bool isConnected(Sublime::Area* area) {
        return m_areas.contains(area);
    }
    
    QString iconName() const {
        return m_iconName;
    }
    
    QIcon activeIcon() const {
        return m_activeIcon;
    }
    QIcon inactiveIcon() const {
        return m_inactiveIcon;
    }
    
    QString id() const {
        return m_id;
    }
    ///Creates a copy of this working-set with a new identity
    WorkingSet* clone() {
        WorkingSet* ret = new WorkingSet(*this);
        return ret;
    }
    
    QStringList fileList() const;
    
    bool isEmpty() const;

    ///Updates this working-set from the given area and area-index
    void saveFromArea(Sublime::Area* area, Sublime::AreaIndex * areaIndex);
    
    ///Loads this working-set directly from the configuration file, and stores it in the given area
    ///@param clear If this is true, the area will be cleared before
    void loadToArea(Sublime::Area* area, Sublime::AreaIndex* areaIndex, bool clear = true);

    void connectArea(Sublime::Area* area) {
        if(m_areas.contains(area)) {
            kDebug() << "tried to double-connect area";
            return;
        }
        kDebug() << "connecting" << m_id << "to area" << area;
//         Q_ASSERT(area->workingSet() == m_id);
        
        m_areas.push_back(area);
        connect(area, SIGNAL(viewAdded(Sublime::AreaIndex*,Sublime::View*)), this, SLOT(areaViewAdded(Sublime::AreaIndex*,Sublime::View*)));
        connect(area, SIGNAL(viewRemoved(Sublime::AreaIndex*,Sublime::View*)), this, SLOT(areaViewRemoved(Sublime::AreaIndex*,Sublime::View*)));
        connect(area, SIGNAL(changingWorkingSet(Sublime::Area*,QString,QString)), this, SLOT(changingWorkingSet(Sublime::Area*,QString,QString)));
        connect(area, SIGNAL(changedWorkingSet(Sublime::Area*,QString,QString)), this, SLOT(changedWorkingSet(Sublime::Area*,QString,QString)));
    }
    
    void disconnectArea(Sublime::Area* area) {

        if(!m_areas.contains(area)) {
            kDebug() << "tried to disconnect not connected area";
            return;
        }
        kDebug() << "disconnecting" << m_id << "from area" << area;
        
//         Q_ASSERT(area->workingSet() == m_id);
        
        disconnect(area, SIGNAL(viewAdded(Sublime::AreaIndex*,Sublime::View*)), this, SLOT(areaViewAdded(Sublime::AreaIndex*,Sublime::View*)));
        disconnect(area, SIGNAL(viewRemoved(Sublime::AreaIndex*,Sublime::View*)), this, SLOT(areaViewRemoved(Sublime::AreaIndex*,Sublime::View*)));
        disconnect(area, SIGNAL(changingWorkingSet(Sublime::Area*,QString,QString)), this, SLOT(changingWorkingSet(Sublime::Area*,QString,QString)));
        disconnect(area, SIGNAL(changedWorkingSet(Sublime::Area*,QString,QString)), this, SLOT(changedWorkingSet(Sublime::Area*,QString,QString)));
        m_areas.removeAll(area);
    }

private slots:
    void areaViewAdded(Sublime::AreaIndex* /*index*/, Sublime::View* /*view*/) ;
    void areaViewRemoved(Sublime::AreaIndex* /*index*/, Sublime::View* /*view*/) ;
    void changingWorkingSet(Sublime::Area* area, QString from, QString to);
    void changedWorkingSet(Sublime::Area*, QString, QString);
    Q_SIGNALS:
    void setChangedSignificantly();
private:
    
    void changed(Sublime::Area* area) {
        if(m_loading)
            return; //Do not capture changes done while loading
        {
            PushValue<bool> enableLoading(m_loading, true);
            
            kDebug() << "recording change done to" << m_id;
            saveFromArea(area, area->rootIndex());
            for(QList< QPointer< Sublime::Area > >::iterator it = m_areas.begin(); it != m_areas.end(); ++it) {
                if((*it) != area) {
                    loadToArea((*it), (*it)->rootIndex());
                }
            }
        }
        
        emit setChangedSignificantly();
    }
    
    void saveFromArea(Sublime::Area* area, Sublime::AreaIndex * areaIndex, KConfigGroup & group);
    void loadToArea(Sublime::Area* area, Sublime::AreaIndex* areaIndex, KConfigGroup group);
    
    WorkingSet(const WorkingSet& rhs) {
        m_id =  rhs.m_id + "_copy_";
    }

    QString m_id;
    QString m_iconName;
    QIcon m_activeIcon, m_inactiveIcon;
    QList<QPointer<Sublime::Area> > m_areas;
    static bool m_loading;
};

class WorkingSetController;

class WorkingSetWidget : public QWidget {
Q_OBJECT
public:
    WorkingSetWidget(KDevelop::MainWindow* parent, WorkingSetController* controller, bool mini, Sublime::Area* fixedArea) ;

private:
    QHBoxLayout* m_layout;
    QPointer<Sublime::Area> m_connectedArea;
    QPointer<Sublime::Area> m_fixedArea;
    bool m_mini;
    QMap<QToolButton*, WorkingSet*> m_buttons;
    MainWindow* m_mainWindow;
    
public slots:
    void areaChanged(Sublime::Area*);
    void changingWorkingSet(Sublime::Area*,QString,QString);
    void workingSetsChanged();
    void buttonTriggered();
};

class WorkingSetController : public QObject
{
Q_OBJECT
public:
    WorkingSetController(KDevelop::Core* core) ;
    ///Returns a working-set management widget
//     QWidget* createManagerWidget(QObject* parent);
    
    WorkingSet* getWorkingSet(QString id);
    void initialize() {}
    void cleanup();
    
    QList<WorkingSet*> allWorkingSets() {
        return m_workingSets.values();
    }

    //The returned widget is owned by the caller
    QWidget* createSetManagerWidget(MainWindow* parent, bool local = false, Sublime::Area* fixedArea = 0) ;

    void initializeController(UiController* controller) {
        connect(controller, SIGNAL(areaCreated(Sublime::Area*)), this, SLOT(areaCreated(Sublime::Area*)));
    }
    
Q_SIGNALS:
    void workingSetAdded(QString id);
    void workingSetRemoved(QString id);
    
private slots:
    void areaCreated(Sublime::Area* area) {
        WorkingSet* set = getWorkingSet(area->workingSet());
        set->connectArea(area);
    }

    bool usingIcon(QString icon);

    bool iconValid(QString icon);
private:
    QSet<QString> m_usedIcons;
    QMap<QString, WorkingSet*> m_workingSets;
    KDevelop::Core* m_core;
};
}

#endif // WORKINGSETMANAGER_H