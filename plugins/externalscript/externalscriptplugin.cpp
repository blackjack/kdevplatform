/*
    This plugin is part of KDevelop.

    Copyright (C) 2010 Milian Wolff <mail@milianw.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "externalscriptplugin.h"

#include "externalscriptview.h"
#include "externalscriptitem.h"
#include "externalscriptjob.h"

#include <interfaces/icore.h>
#include <interfaces/iuicontroller.h>
#include <interfaces/iruncontroller.h>

#include <outputview/outputjob.h>
#include <outputview/outputmodel.h>

#include <KPluginFactory>
#include <KAboutData>

#include <QStandardItemModel>
#include <KAction>
#include <interfaces/isession.h>

K_PLUGIN_FACTORY( ExternalScriptFactory, registerPlugin<ExternalScriptPlugin>(); )
K_EXPORT_PLUGIN( ExternalScriptFactory( KAboutData( "kdevexternalscript", "kdevexternalscript", ki18n( "External Scripts" ),
                                        "0.1", ki18n( "Support for running external scripts." ),
                                        KAboutData::License_GPL ) ) )

class ExternalScriptViewFactory: public KDevelop::IToolViewFactory
{
public:
  ExternalScriptViewFactory( ExternalScriptPlugin *plugin ): m_plugin( plugin ) {}

  virtual QWidget* create( QWidget *parent = 0 ) {
    return new ExternalScriptView( m_plugin, parent );
  }

  virtual Qt::DockWidgetArea defaultPosition() {
    return Qt::RightDockWidgetArea;
  }

  virtual QString id() const {
    return "org.kdevelop.ExternalScriptView";
  }

private:
  ExternalScriptPlugin *m_plugin;
};

ExternalScriptPlugin* ExternalScriptPlugin::m_self = 0;

ExternalScriptPlugin::ExternalScriptPlugin( QObject* parent, const QVariantList& /*args*/ )
    : IPlugin( ExternalScriptFactory::componentData(), parent ),
    m_model( new QStandardItemModel( this ) ), m_factory( new ExternalScriptViewFactory( this ) )
{
  Q_ASSERT( !m_self );
  m_self = this;

  setXMLFile( "kdevexternalscript.rc" );

  //BEGIN load config
  KConfigGroup config = getConfig();
  foreach( const QString group, config.groupList() ) {
    KConfigGroup script = config.group( group );
    if ( script.hasKey( "name" ) && script.hasKey( "command" ) ) {
      ExternalScriptItem* item = new ExternalScriptItem;
      item->setText( script.readEntry( "name" ) );
      item->setCommand( script.readEntry( "command" ));
      item->setInputMode( static_cast<ExternalScriptItem::InputMode>( script.readEntry( "inputMode", 0u ) ) );
      item->setReplaceMode( static_cast<ExternalScriptItem::ReplaceMode>( script.readEntry( "replaceMode", 0u ) ) );
      item->setSaveMode( static_cast<ExternalScriptItem::SaveMode>( script.readEntry( "saveMode", 0u ) ) );
      item->action()->setShortcut( KShortcut( script.readEntry( "shortcuts" ) ) );
      m_model->appendRow( item );
    }
  }
  //END load config

  core()->uiController()->addToolView( i18n( "External Scripts" ), m_factory );

  connect( m_model, SIGNAL(rowsRemoved(QModelIndex, int, int)),
           this, SLOT(rowsRemoved(QModelIndex, int, int)) );
  connect( m_model, SIGNAL(rowsInserted(QModelIndex, int, int)),
           this, SLOT(rowsInserted(QModelIndex, int, int)) );

  const bool firstUse = config.readEntry( "firstUse", true );
  if ( firstUse ) {
    // some example scripts
    ExternalScriptItem* item = new ExternalScriptItem;
    item->setText( i18n("quick compile") );
    item->setCommand( "g++ -o %b %f && ./%b" );
    m_model->appendRow( item );

    item = new ExternalScriptItem;
    item->setText( i18n("google selection") );
    item->setCommand( "xdg-open \"http://www.google.de/search?q=%s\"" );
    m_model->appendRow( item );

    item = new ExternalScriptItem;
    item->setText( i18n("sort selection") );
    item->setCommand( "sort" );
    item->setInputMode( ExternalScriptItem::InputSelection );
    item->setReplaceMode( ExternalScriptItem::ReplaceSelection );
    m_model->appendRow( item );

    config.writeEntry( "firstUse", false );
    config.sync();
  }
}

ExternalScriptPlugin* ExternalScriptPlugin::self()
{
  return m_self;
}

ExternalScriptPlugin::~ExternalScriptPlugin()
{
  m_self = 0;
}

void ExternalScriptPlugin::unload()
{
  core()->uiController()->removeToolView( m_factory );
  KDevelop::IPlugin::unload();
}

KConfigGroup ExternalScriptPlugin::getConfig() const
{
  return KGlobal::config()->group("External Scripts");
}

QStandardItemModel* ExternalScriptPlugin::model() const
{
  return m_model;
}

void ExternalScriptPlugin::execute( ExternalScriptItem* item ) const
{
  ExternalScriptJob* job = new ExternalScriptJob( item, const_cast<ExternalScriptPlugin*>(this) );

  KDevelop::ICore::self()->runController()->registerJob( job );
}

void ExternalScriptPlugin::executeScriptFromActionData() const
{
  KAction* action = dynamic_cast<KAction*>( sender() );
  Q_ASSERT( action );

  ExternalScriptItem* item = action->data().value<ExternalScriptItem*>();
  Q_ASSERT( item );

  execute( item );
}

void ExternalScriptPlugin::rowsInserted( const QModelIndex& /*parent*/, int start, int end )
{
  for ( int i = start; i <= end; ++i ) {
    saveItemForRow( i );
  }
}

void ExternalScriptPlugin::rowsRemoved( const QModelIndex& /*parent*/, int start, int end )
{
  KConfigGroup config = getConfig();
  for ( int i = start; i <= end; ++i ) {
    KConfigGroup child = config.group( QString("script %1").arg(i) );
    qDebug() << "removing config group:" << child.name();
    child.deleteGroup();
  }
  config.sync();
}

void ExternalScriptPlugin::saveItem( const ExternalScriptItem* item )
{
  const QModelIndex index = m_model->indexFromItem( item );
  Q_ASSERT( index.isValid() );
  saveItemForRow( index.row() );
}

void ExternalScriptPlugin::saveItemForRow( int row )
{
  const QModelIndex idx = m_model->index( row, 0 );
  Q_ASSERT( idx.isValid() );

  ExternalScriptItem* item = dynamic_cast<ExternalScriptItem*>( m_model->item( row ) );
  Q_ASSERT( item );

  qDebug() << "save extern script:" << item << idx;
  KConfigGroup config = getConfig().group( QString("script %1").arg( row ) );
  config.writeEntry( "name", item->text() );
  config.writeEntry( "command", item->command() );
  config.writeEntry( "inputMode", (uint) item->inputMode() );
  config.writeEntry( "replaceMode", (uint) item->replaceMode() );
  config.writeEntry( "saveMode", (uint) item->saveMode() );
  config.writeEntry( "shortcuts", item->action()->shortcut().toString() );
  config.sync();
}

#include "externalscriptplugin.moc"

// kate: indent-mode cstyle; space-indent on; indent-width 2; replace-tabs on; 