/*
 * This file is part of KDevelop
 *
 * Copyright 2007 David Nolden <david.nolden.kdevelop@art-master.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "quickopenplugin.h"

#include <cassert>
#include <typeinfo>
#include <QtGui/QTreeView>
#include <QtGui/QHeaderView>
#include <QDialog>
#include <QKeyEvent>
#include <QApplication>
#include <QScrollBar>
#include <QCheckBox>
#include <QMetaObject>

#include <kbuttongroup.h>
#include <klocale.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <kaboutdata.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <kparts/mainwindow.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <kshortcut.h>
#include <kdebug.h>

#include <interfaces/ilanguage.h>
#include <interfaces/icore.h>
#include <interfaces/iuicontroller.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <language/interfaces/ilanguagesupport.h>
#include <language/editor/hashedstring.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchain.h>
#include <language/duchain/types/identifiedtype.h>
#include <language/duchain/indexedstring.h>
#include <language/duchain/types/functiontype.h>

#include "expandingtree/expandingdelegate.h"
#include "ui_quickopen.h"
#include "quickopenmodel.h"
#include "projectfilequickopen.h"
#include "projectitemquickopen.h"
#include "declarationlistquickopen.h"
#include "customlistquickopen.h"
#include <language/duchain/functiondefinition.h>
#include <qmenu.h>
#include <qdesktopwidget.h>
#include <util/activetooltip.h>
#include <qboxlayout.h>
#include <language/util/navigationtooltip.h>

using namespace KDevelop;

const bool noHtmlDestriptionInOutline = true;

class QuickOpenDelegate : public ExpandingDelegate {
public:
  QuickOpenDelegate(ExpandingWidgetModel* model, QObject* parent = 0L) : ExpandingDelegate(model, parent) {
  }
  virtual QList<QTextLayout::FormatRange> createHighlighting(const QModelIndex& index, QStyleOptionViewItem& option) const {
    QList<QVariant> highlighting = index.data(KTextEditor::CodeCompletionModel::CustomHighlight).toList();
    if(!highlighting.isEmpty())
      return highlightingFromVariantList(highlighting);
    return ExpandingDelegate::createHighlighting( index, option );
  }

};

K_PLUGIN_FACTORY(KDevQuickOpenFactory, registerPlugin<QuickOpenPlugin>(); )
K_EXPORT_PLUGIN(KDevQuickOpenFactory(KAboutData("kdevquickopen","kdevquickopen", ki18n("Quick Open"), "0.1", ki18n("Quickly open resources such as files, classes and methods."), KAboutData::License_GPL)))

Declaration* cursorDeclaration() {
  IDocument* doc = ICore::self()->documentController()->activeDocument();
  if(!doc)
    return 0;

  KTextEditor::Document* textDoc = doc->textDocument();
  if(!textDoc)
    return 0;

  KTextEditor::View* view = textDoc->activeView();
  if(!view)
    return 0;

  KDevelop::DUChainReadLocker lock( DUChain::lock() );

  return DUChainUtils::declarationForDefinition( DUChainUtils::itemUnderCursor( doc->url(), SimpleCursor(view->cursorPosition()) ) );
}

///The first definition that belongs to a context that surrounds the current cursor
Declaration* cursorContextDeclaration() {
  IDocument* doc = ICore::self()->documentController()->activeDocument();
  if(!doc)
    return 0;

  KTextEditor::Document* textDoc = doc->textDocument();
  if(!textDoc)
    return 0;

  KTextEditor::View* view = textDoc->activeView();
  if(!view)
    return 0;

  KDevelop::DUChainReadLocker lock( DUChain::lock() );

  TopDUContext* ctx = DUChainUtils::standardContextForUrl(doc->url());
  if(!ctx)
    return 0;

  SimpleCursor cursor(view->cursorPosition());

  DUContext* subCtx = ctx->findContext(cursor);

  while(subCtx && !subCtx->owner())
    subCtx = subCtx->parentContext();

  Declaration* definition = 0;

  if(!subCtx || !subCtx->owner())
    definition = DUChainUtils::declarationInLine(cursor, ctx);
  else
    definition = subCtx->owner();

  if(!definition)
    return 0;

  return definition;
}

//Returns only the name, no template-parameters or scope
QString cursorItemText() {
  KDevelop::DUChainReadLocker lock( DUChain::lock() );

  Declaration* decl = cursorDeclaration();
  if(!decl)
    return QString();

  IDocument* doc = ICore::self()->documentController()->activeDocument();
  if(!doc)
    return QString();

  TopDUContext* context = DUChainUtils::standardContextForUrl( doc->url() );

  if( !context ) {
    kDebug() << "Got no standard context";
    return QString();
  }

  AbstractType::Ptr t = decl->abstractType();
  IdentifiedType* idType = dynamic_cast<IdentifiedType*>(t.unsafeData());
  if( idType && idType->declaration(context) )
    decl = idType->declaration(context);

  if(!decl->qualifiedIdentifier().isEmpty())
    return decl->qualifiedIdentifier().last().identifier().str();

  return QString();
}

QWidget* QuickOpenPlugin::createQuickOpenLineWidget()
{
  return new QuickOpenLineEdit;
}

void QuickOpenWidget::showStandardButtons(bool show)
{
  if(show) {
    o.okButton->show();
    o.cancelButton->show();
  }else{
    o.okButton->hide();
    o.cancelButton->hide();
  }
}

QuickOpenWidget::QuickOpenWidget( QString title, QuickOpenModel* model, const QStringList& initialItems, const QStringList& initialScopes, bool listOnly, bool noSearchField ) : m_model(model), m_expandedTemporary(false) {

  o.setupUi( this );
  o.list->header()->hide();
  o.list->setRootIsDecorated( false );
  o.list->setVerticalScrollMode( QAbstractItemView::ScrollPerItem );
  
  connect(o.list->verticalScrollBar(), SIGNAL(valueChanged(int)), m_model, SLOT(placeExpandingWidgets()));

  o.searchLine->setFocus();

  o.list->setItemDelegate( new QuickOpenDelegate( m_model, o.list ) );

  if(!listOnly) {
    QStringList allTypes = m_model->allTypes();
    QStringList allScopes = m_model->allScopes();

    QMenu* itemsMenu = new QMenu;
    
    foreach( const QString &type, allTypes )
    {
      QAction* action = new QAction(type, itemsMenu);
      action->setCheckable(true);
      action->setChecked(initialItems.isEmpty() || initialItems.contains( type ));
      connect( action, SIGNAL(toggled(bool)), this, SLOT(updateProviders()), Qt::QueuedConnection );
      itemsMenu->addAction(action);
    }

    o.itemsButton->setMenu(itemsMenu);

    QMenu* scopesMenu = new QMenu;

    foreach( const QString &scope, allScopes )
    {
      QAction* action = new QAction(scope, scopesMenu);
      action->setCheckable(true);
      action->setChecked(initialScopes.isEmpty() || initialScopes.contains( scope ) );

      connect( action, SIGNAL(toggled(bool)), this, SLOT(updateProviders()), Qt::QueuedConnection );
      scopesMenu->addAction(action);
    }
    
    o.scopesButton->setMenu(scopesMenu);

  }else{
    o.list->setFocusPolicy(Qt::StrongFocus);
    o.scopesButton->hide();
    o.itemsButton->hide();
    o.label->hide();
    o.label_2->hide();
  }

  showSearchField(!noSearchField);
  
  o.okButton->hide();
  o.cancelButton->hide();
  
  o.searchLine->installEventFilter( this );
  o.list->installEventFilter( this );
  o.list->setFocusPolicy(Qt::NoFocus);
  o.scopesButton->setFocusPolicy(Qt::NoFocus);
  o.itemsButton->setFocusPolicy(Qt::NoFocus);

  connect( o.searchLine, SIGNAL(textChanged( const QString& )), this, SLOT(textChanged( const QString& )) );

  connect( o.list, SIGNAL(doubleClicked( const QModelIndex& )), this, SLOT(doubleClicked( const QModelIndex& )) );

  connect(o.okButton, SIGNAL(clicked(bool)), this, SLOT(accept()));
  connect(o.okButton, SIGNAL(clicked(bool)), SIGNAL(ready()));
  connect(o.cancelButton, SIGNAL(clicked(bool)), SIGNAL(ready()));
  
  updateProviders();

  m_model->restart();

  o.list->setColumnWidth( 0, 20 );
}


void QuickOpenWidget::setAlternativeSearchField(QLineEdit* alterantiveSearchField)
{
    o.searchLine = alterantiveSearchField;
    o.searchLine->installEventFilter( this );
    connect( o.searchLine, SIGNAL(textChanged( const QString& )), this, SLOT(textChanged( const QString& )) );
}


void QuickOpenWidget::showSearchField(bool b)
{
    if(b){
      o.searchLine->show();
      o.searchLabel->show();
    }else{
      o.searchLine->hide();
      o.searchLabel->hide();
    }
}

void QuickOpenWidget::prepareShow()
{
  o.list->setModel( 0 );
  o.list->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
  m_model->setTreeView( o.list );
  o.list->setModel( m_model );
  if (!m_preselectedText.isEmpty())
  {
    o.searchLine->setText(m_preselectedText);
    o.searchLine->selectAll();
  }
  connect( o.list->selectionModel(), SIGNAL(currentRowChanged( const QModelIndex&, const QModelIndex& )), this, SLOT(currentChanged( const QModelIndex&, const QModelIndex& )) );
  connect( o.list->selectionModel(), SIGNAL(selectionChanged( const QItemSelection&, const QItemSelection& )), this, SLOT(currentChanged( const QItemSelection&, const QItemSelection& )) );
}

void QuickOpenWidgetDialog::run() {
  m_widget->prepareShow();
  m_dialog->show();
}

QuickOpenWidget::~QuickOpenWidget() {
  m_model->setTreeView( 0 );
}


QuickOpenWidgetDialog::QuickOpenWidgetDialog(QString title, QuickOpenModel* model, const QStringList& initialItems, const QStringList& initialScopes, bool listOnly, bool noSearchField)
{
  m_widget = new QuickOpenWidget(title, model, initialItems, initialScopes, listOnly, noSearchField);
  
  //KDialog always sets the focus on the "OK" button, so we use QDialog
  m_dialog = new QDialog( ICore::self()->uiController()->activeMainWindow() );
  m_dialog->resize(QSize(800, 400));

  m_dialog->setWindowTitle(title);
  QVBoxLayout* layout = new QVBoxLayout(m_dialog);
  layout->addWidget(m_widget);
  m_widget->showStandardButtons(true);
  connect(m_widget, SIGNAL(ready()), m_dialog, SLOT(close()));
  connect( m_dialog, SIGNAL(accepted()), m_widget, SLOT(accept()) );
}


QuickOpenWidgetDialog::~QuickOpenWidgetDialog()
{
  delete m_dialog;
}

void QuickOpenWidget::setPreselectedText(const QString& text)
{
  m_preselectedText = text;
}

void QuickOpenWidget::updateProviders() {
  if(QAction* action = qobject_cast<QAction*>(sender())) {
    QMenu* menu = qobject_cast<QMenu*>(action->parentWidget());
    if(menu) {
      menu->show();
      menu->setActiveAction(action);
    }
  }
  
  QStringList checkedItems;
  
  if(o.itemsButton->menu()) {
  
    foreach( QObject* obj, o.itemsButton->menu()->children() ) {
      QAction* box = qobject_cast<QAction*>( obj );
      if( box ) {
        if( box->isChecked() )
          checkedItems << box->text().remove('&');
      }
    }
    o.itemsButton->setText(checkedItems.join(", "));
  }

  QStringList checkedScopes;
  
  if(o.scopesButton->menu()) {
    
    foreach( QObject* obj, o.scopesButton->menu()->children() ) {
      QAction* box = qobject_cast<QAction*>( obj );
      if( box ) {
        if( box->isChecked() )
          checkedScopes << box->text().remove('&');
      }
    }
    
    o.scopesButton->setText(checkedScopes.join(", "));
  }

  emit itemsChanged( checkedItems );
  emit scopesChanged( checkedScopes );
  m_model->enableProviders( checkedItems, checkedScopes );
}


void QuickOpenWidget::textChanged( const QString& str ) {
  m_model->textChanged( str );

  QModelIndex currentIndex = m_model->index(0, 0, QModelIndex());
  o.list->selectionModel()->setCurrentIndex( currentIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current );

  callRowSelected();
}

void QuickOpenWidget::callRowSelected() {
  QModelIndex currentIndex = o.list->selectionModel()->currentIndex();
  if( currentIndex.isValid() )
    m_model->rowSelected( currentIndex );
  else
    kDebug() << "current index is not valid";
}

void QuickOpenWidget::currentChanged( const QModelIndex& /*current*/, const QModelIndex& /*previous */) {
  callRowSelected();
}

void QuickOpenWidget::currentChanged( const QItemSelection& /*current*/, const QItemSelection& /*previous */) {
  callRowSelected();
}

void QuickOpenWidget::accept() {
  QString filterText = o.searchLine->text();
  m_model->execute( o.list->currentIndex(), filterText );
}

void QuickOpenWidget::doubleClicked ( const QModelIndex & index ) {
  QString filterText = o.searchLine->text();
  if(  m_model->execute( index, filterText ) )
    emit ready();
  else if( filterText != o.searchLine->text() )
    o.searchLine->setText( filterText );
}


bool QuickOpenWidget::eventFilter ( QObject * watched, QEvent * event )
{
  QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>(event);
  
  if( event->type() == QEvent::KeyRelease ) {
    if(keyEvent->key() == Qt::Key_Alt) {
      if((m_expandedTemporary && m_altDownTime.msecsTo( QTime::currentTime() ) > 300) || (!m_expandedTemporary && m_altDownTime.msecsTo( QTime::currentTime() ) < 300 && m_hadNoCommandSinceAlt)) {
        //Unexpand the item
        QModelIndex row = o.list->selectionModel()->currentIndex();
        if( row.isValid() ) {
          row = row.sibling( row.row(), 0 );
          if(m_model->isExpanded( row ))
            m_model->setExpanded( row, false );
        }      
      }
      m_expandedTemporary = false;
    }
  }
  
  if( event->type() == QEvent::KeyPress  ) {
    m_hadNoCommandSinceAlt = false;
    if(keyEvent->key() == Qt::Key_Alt) {
      m_hadNoCommandSinceAlt = true;
      //Expand
      QModelIndex row = o.list->selectionModel()->currentIndex();
      if( row.isValid() ) {
        row = row.sibling( row.row(), 0 );
        m_altDownTime = QTime::currentTime();
        if(!m_model->isExpanded( row )) {
          m_expandedTemporary = true;
          m_model->setExpanded( row, true );
        }
      }      
    }

    switch( keyEvent->key() ) {
      case Qt::Key_Down:
      case Qt::Key_Up:
      {
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          QWidget* w = m_model->expandingWidget(o.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            if( keyEvent->key() == Qt::Key_Down )
              interface->down();
            else
              interface->up();
            return true;
          }
          return false;
        }
      }
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        if(watched == o.list )
          return false;
        QApplication::sendEvent( o.list, event );
      //callRowSelected();
        return true;

      case Qt::Key_Left: {
        //Expand/unexpand
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          //Eventually Send action to the widget
          QWidget* w = m_model->expandingWidget(o.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            interface->previous();
            return true;
          }
        } else {
          QModelIndex row = o.list->selectionModel()->currentIndex();
          if( row.isValid() ) {
            row = row.sibling( row.row(), 0 );

            if( m_model->isExpanded( row ) ) {
              m_model->setExpanded( row, false );
              return true;
            }
          }
        }
        return false;
      }
      case Qt::Key_Right: {
        //Expand/unexpand
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          //Eventually Send action to the widget
          QWidget* w = m_model->expandingWidget(o.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            interface->next();
            return true;
          }
        } else {
          QModelIndex row = o.list->selectionModel()->currentIndex();
          if( row.isValid() ) {
            row = row.sibling( row.row(), 0 );

            if( !m_model->isExpanded( row ) ) {
              m_model->setExpanded( row, true );
              return true;
            }
          }
        }
        return false;
      }
      case Qt::Key_Return:
      case Qt::Key_Enter: {
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          //Eventually Send action to the widget
          QWidget* w = m_model->expandingWidget(o.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            interface->accept();
            return true;
          }
        } else {
          QString filterText = o.searchLine->text();
          if( m_model->execute( o.list->currentIndex(), filterText ) ) {
            if(!(keyEvent->modifiers() & Qt::ShiftModifier))
              emit ready();
          } else {
            //Maybe the filter-text was changed:
            if( filterText != o.searchLine->text() ) {
              o.searchLine->setText( filterText );
            }
          }
        }
        return true;
      }
    }
  }

  return false;
}


QuickOpenLineEdit* QuickOpenPlugin::quickOpenLine()
{
  QList< QuickOpenLineEdit* > lines = ICore::self()->uiController()->activeMainWindow()->findChildren<QuickOpenLineEdit*>("Quickopen");
  foreach(QuickOpenLineEdit* line, lines) {
    if(line->isVisible()) {
      return line;
    }
  }
  
  return 0;
}

static QuickOpenPlugin* staticQuickOpenPlugin = 0;

QuickOpenPlugin* QuickOpenPlugin::self()
{
  return staticQuickOpenPlugin;
}

QuickOpenPlugin::QuickOpenPlugin(QObject *parent,
                                 const QVariantList&)
    : KDevelop::IPlugin(KDevQuickOpenFactory::componentData(), parent)
{
    staticQuickOpenPlugin = this;
    KDEV_USE_EXTENSION_INTERFACE( KDevelop::IQuickOpen )
    setXMLFile( "kdevquickopen.rc" );
    m_model = new QuickOpenModel( 0 );

    KActionCollection* actions = actionCollection();

    KAction* quickOpen = actions->addAction("quick_open");
    quickOpen->setText( i18n("&Quick Open") );
    quickOpen->setShortcut( Qt::CTRL | Qt::ALT | Qt::Key_Q );
    connect(quickOpen, SIGNAL(triggered(bool)), this, SLOT(quickOpen()));

    KAction* quickOpenFile = actions->addAction("quick_open_file");
    quickOpenFile->setText( i18n("Quick Open &File") );
    quickOpenFile->setShortcut( Qt::CTRL | Qt::ALT | Qt::Key_O );
    connect(quickOpenFile, SIGNAL(triggered(bool)), this, SLOT(quickOpenFile()));

    KAction* quickOpenClass = actions->addAction("quick_open_class");
    quickOpenClass->setText( i18n("Quick Open &Class") );
    quickOpenClass->setShortcut( Qt::CTRL | Qt::ALT | Qt::Key_C );
    connect(quickOpenClass, SIGNAL(triggered(bool)), this, SLOT(quickOpenClass()));

    KAction* quickOpenFunction = actions->addAction("quick_open_function");
    quickOpenFunction->setText( i18n("Quick Open &Function") );
    quickOpenFunction->setShortcut( Qt::CTRL | Qt::ALT | Qt::Key_M );
    connect(quickOpenFunction, SIGNAL(triggered(bool)), this, SLOT(quickOpenFunction()));

    KAction* quickOpenDeclaration = actions->addAction("quick_open_jump_declaration");
    quickOpenDeclaration->setText( i18n("Jump to Declaration") );
    quickOpenDeclaration->setShortcut( Qt::CTRL | Qt::Key_Period );
    connect(quickOpenDeclaration, SIGNAL(triggered(bool)), this, SLOT(quickOpenDeclaration()));

    KAction* quickOpenDefinition = actions->addAction("quick_open_jump_definition");
    quickOpenDefinition->setText( i18n("Jump to Definition") );
    quickOpenDefinition->setShortcut( Qt::CTRL | Qt::Key_Comma );
    connect(quickOpenDefinition, SIGNAL(triggered(bool)), this, SLOT(quickOpenDefinition()));

    KAction* quickOpenLine = actions->addAction("quick_open_line");
    quickOpenLine->setText( i18n("Embedded Quick Open") );
//     quickOpenLine->setShortcut( Qt::CTRL | Qt::ALT | Qt::Key_E );
//     connect(quickOpenLine, SIGNAL(triggered(bool)), this, SLOT(quickOpenLine(bool)));
    quickOpenLine->setDefaultWidget(createQuickOpenLineWidget());
//     KAction* quickOpenNavigate = actions->addAction("quick_open_navigate");
//     quickOpenNavigate->setText( i18n("Navigate Declaration") );
//     quickOpenNavigate->setShortcut( Qt::ALT | Qt::Key_Space );
//     connect(quickOpenNavigate, SIGNAL(triggered(bool)), this, SLOT(quickOpenNavigate()));

    KAction* quickOpenNavigateFunctions = actions->addAction("quick_open_outline");
    quickOpenNavigateFunctions->setText( i18n("Outline") );
    quickOpenNavigateFunctions->setShortcut( Qt::CTRL| Qt::ALT | Qt::Key_N );
    connect(quickOpenNavigateFunctions, SIGNAL(triggered(bool)), this, SLOT(quickOpenNavigateFunctions()));
    KConfigGroup quickopengrp = KGlobal::config()->group("QuickOpen");
    lastUsedScopes = quickopengrp.readEntry("SelectedScopes", QStringList() << i18n("Project") << i18n("Includes") << i18n("Includers") << i18n("Currently Open") );
    lastUsedItems = quickopengrp.readEntry("SelectedItems", QStringList() );

    {
      m_openFilesData = new OpenFilesDataProvider();
      QStringList scopes, items;
      scopes << i18n("Currently Open");
      items << i18n("Files");
      m_model->registerProvider( scopes, items, m_openFilesData );
    }
    {
      m_projectFileData = new ProjectFileDataProvider();
      QStringList scopes, items;
      scopes << i18n("Project");
      items << i18n("Files");
      m_model->registerProvider( scopes, items, m_projectFileData );
    }
    {
      m_projectItemData = new ProjectItemDataProvider(this);
      QStringList scopes, items;
      scopes << i18n("Project");
      items << ProjectItemDataProvider::supportedItemTypes();
      m_model->registerProvider( scopes, items, m_projectItemData );
    }

}

QuickOpenPlugin::~QuickOpenPlugin()
{
  freeModel();

  delete m_model;
  delete m_projectFileData;
  delete m_projectItemData;
  delete m_openFilesData;
}

void QuickOpenPlugin::unload()
{
}

void QuickOpenPlugin::showQuickOpen( ModelTypes modes )
{
  if(!freeModel())
    return;

  QStringList initialItems;
  if( modes & Files || modes & OpenFiles )
    initialItems << i18n("Files");

  if( modes & Functions )
    initialItems << i18n("Functions");

  if( modes & Classes )
    initialItems << i18n("Classes");
  
  QStringList useScopes = lastUsedScopes;
  
  if((modes & OpenFiles) && !useScopes.contains(i18n("Currently Open")))
    useScopes << i18n("Currently Open");

  QuickOpenWidgetDialog* dialog = new QuickOpenWidgetDialog( i18n("Quick Open"), m_model, initialItems, useScopes );
  m_currentWidgetHandler = dialog;
  if (!(modes & Files) || modes == QuickOpenPlugin::All)
  {
    KDevelop::IDocument *currentDoc = core()->documentController()->activeDocument();
    if (currentDoc && currentDoc->isTextDocument())
    {
      QString preselected = currentDoc->textSelection().isEmpty() ? currentDoc->textWord() : currentDoc->textDocument()->text(currentDoc->textSelection());
      dialog->widget()->setPreselectedText(preselected);
    }
  }
  
  connect( m_currentWidgetHandler, SIGNAL( scopesChanged( const QStringList& ) ), this, SLOT( storeScopes( const QStringList& ) ) );
  connect( m_currentWidgetHandler, SIGNAL( itemsChanged( const QStringList& ) ), this, SLOT( storeItems( const QStringList& ) ) );
  
  if(quickOpenLine()) {
    quickOpenLine()->showWithWidget(dialog->widget());
    dialog->deleteLater();
  }else{
    dialog->run();
  }
}


void QuickOpenPlugin::storeScopes( const QStringList& scopes )
{
  lastUsedScopes = scopes;
  KConfigGroup grp = KGlobal::config()->group("QuickOpen");
  grp.writeEntry( "SelectedScopes", scopes );
}

void QuickOpenPlugin::storeItems( const QStringList& items )
{
  lastUsedItems = items;
  KConfigGroup grp = KGlobal::config()->group("QuickOpen");
  grp.writeEntry( "SelectedItems", items );
}

void QuickOpenPlugin::quickOpen()
{
  showQuickOpen( All );
}

void QuickOpenPlugin::quickOpenFile()
{
  showQuickOpen( (ModelTypes)(Files | OpenFiles) );
}

void QuickOpenPlugin::quickOpenFunction()
{
  showQuickOpen( Functions );
}

void QuickOpenPlugin::quickOpenClass()
{
  showQuickOpen( Classes );
}

QSet<KDevelop::IndexedString> QuickOpenPlugin::fileSet() const {
  return m_model->fileSet();
}

void QuickOpenPlugin::registerProvider( const QStringList& scope, const QStringList& type, KDevelop::QuickOpenDataProviderBase* provider )
{
  m_model->registerProvider( scope, type, provider );
}

bool QuickOpenPlugin::removeProvider( KDevelop::QuickOpenDataProviderBase* provider )
{
  m_model->removeProvider( provider );
  return true;
}

void QuickOpenPlugin::quickOpenDeclaration()
{
  if(jumpToSpecialObject())
    return;

  KDevelop::DUChainReadLocker lock( DUChain::lock() );
  Declaration* decl = cursorDeclaration();

  if(!decl) {
    kDebug() << "Found no declaration for cursor, cannot jump";
    return;
  }
  decl->activateSpecialization();

  IndexedString u = decl->url();
  SimpleCursor c = decl->range().start;

  if(u.str().isEmpty()) {
    kDebug() << "Got empty url for declaration" << decl->toString();
    return;
  }

  lock.unlock();
  core()->documentController()->openDocument(KUrl(u.str()), c.textCursor());
}

///Returns all languages for that url that have a language support, and prints warnings for other ones.
QList<KDevelop::ILanguage*> languagesWithSupportForUrl(KUrl url) {
  QList<KDevelop::ILanguage*> languages = ICore::self()->languageController()->languagesForUrl(url);
  QList<KDevelop::ILanguage*> ret;
  foreach( KDevelop::ILanguage* language, languages) {
    if(language->languageSupport()) {
      ret << language;
    }else{
      kDebug() << "got no language-support for language" << language->name();
    }
  }
  return ret;
}

QWidget* QuickOpenPlugin::specialObjectNavigationWidget() const
{
  if( !ICore::self()->documentController()->activeDocument() || !ICore::self()->documentController()->activeDocument()->textDocument() || !ICore::self()->documentController()->activeDocument()->textDocument()->activeView() )
    return false;

  KUrl url = ICore::self()->documentController()->activeDocument()->url();

  foreach( KDevelop::ILanguage* language, languagesWithSupportForUrl(url) ) {
    QWidget* w = language->languageSupport()->specialLanguageObjectNavigationWidget(url, SimpleCursor(ICore::self()->documentController()->activeDocument()->textDocument()->activeView()->cursorPosition()) );
    if(w)
      return w;
  }

  return 0;
}

QPair<KUrl, SimpleCursor> QuickOpenPlugin::specialObjectJumpPosition() const {
  if( !ICore::self()->documentController()->activeDocument() || !ICore::self()->documentController()->activeDocument()->textDocument() || !ICore::self()->documentController()->activeDocument()->textDocument()->activeView() )
    return qMakePair(KUrl(), SimpleCursor());

  KUrl url = ICore::self()->documentController()->activeDocument()->url();

  foreach( KDevelop::ILanguage* language, languagesWithSupportForUrl(url) ) {
    QPair<KUrl, SimpleCursor> pos = language->languageSupport()->specialLanguageObjectJumpCursor(url, SimpleCursor(ICore::self()->documentController()->activeDocument()->textDocument()->activeView()->cursorPosition()) );
    if(pos.second.isValid()) {
      return pos;
    }
  }

  return qMakePair(KUrl(), SimpleCursor::invalid());
}

bool QuickOpenPlugin::jumpToSpecialObject()
{
  QPair<KUrl, SimpleCursor> pos = specialObjectJumpPosition();
  if(pos.second.isValid()) {
    if(pos.first.isEmpty()) {
      kDebug() << "Got empty url for special language object";
      return false;
    }

    ICore::self()->documentController()->openDocument(pos.first, pos.second.textCursor());
    return true;
  }
  return false;
}

void QuickOpenPlugin::quickOpenDefinition()
{
  if(jumpToSpecialObject())
    return;

  KDevelop::DUChainReadLocker lock( DUChain::lock() );
  Declaration* decl = cursorDeclaration();

  if(!decl) {
    kDebug() << "Found no declaration for cursor, cannot jump";
    return;
  }

  IndexedString u = decl->url();
  SimpleCursor c = decl->range().start;
  if(FunctionDefinition* def = FunctionDefinition::definition(decl)) {
    def->activateSpecialization();
    u = def->url();
    c = def->range().start;
  }else{
    kDebug() << "Found no definition for declaration";
    decl->activateSpecialization();
  }

  if(u.str().isEmpty()) {
    kDebug() << "Got empty url for declaration" << decl->toString();
    return;
  }

  lock.unlock();
  core()->documentController()->openDocument(KUrl(u.str()), c.textCursor());
}


void QuickOpenPlugin::quickOpenNavigate()
{
  if(!freeModel())
    return;
  KDevelop::DUChainReadLocker lock( DUChain::lock() );

  QWidget* widget = specialObjectNavigationWidget();
  Declaration* decl = cursorDeclaration();

  if(widget || decl) {

    QuickOpenModel* model = new QuickOpenModel(0);
    model->setExpandingWidgetHeightIncrease(200); //Make the widget higher, since it's the only visible item

    if(widget) {
      QPair<KUrl, SimpleCursor> jumpPos = specialObjectJumpPosition();

      CustomItem item;
      item.m_widget = widget;
      item.m_executeTargetPosition = jumpPos.second;
      item.m_executeTargetUrl = jumpPos.first;

      QList<CustomItem> items;
      items << item;

      model->registerProvider( QStringList(), QStringList(), new CustomItemDataProvider(items) );
    }else{
      DUChainItem item;

      item.m_item = IndexedDeclaration(decl);
      item.m_text = decl->qualifiedIdentifier().toString();

      QList<DUChainItem> items;
      items << item;

      model->registerProvider( QStringList(), QStringList(), new DeclarationListDataProvider(this, items) );
    }

    //Change the parent so there are no conflicts in destruction order
    QuickOpenWidgetDialog* dialog = new QuickOpenWidgetDialog( i18n("Navigate"), model, QStringList(), QStringList(), true, true );
    m_currentWidgetHandler = dialog;
    model->setParent(m_currentWidgetHandler);
    model->setExpanded(model->index(0,0, QModelIndex()), true);

    dialog->run();
  }

  if(!decl) {
    kDebug() << "Found no declaration for cursor, cannot navigate";
    return;
  }
}

bool QuickOpenPlugin::freeModel()
{
  if(m_currentWidgetHandler)
    delete m_currentWidgetHandler;
  m_currentWidgetHandler = 0;

  return true;
}

void QuickOpenPlugin::quickOpenNavigateFunctions()
{
  if(!freeModel())
    return;

  IDocument* doc = ICore::self()->documentController()->activeDocument();
  if(!doc) {
    kDebug() << "No active document";
    return;
  }

  KDevelop::DUChainReadLocker lock( DUChain::lock() );

  TopDUContext* context = DUChainUtils::standardContextForUrl( doc->url() );

  if( !context ) {
    kDebug() << "Got no standard context";
    return;
  }

  QuickOpenModel* model = new QuickOpenModel(0);

  QList<DUChainItem> items;

  class OutlineFilter : public DUChainUtils::DUChainItemFilter {
  public:
    OutlineFilter(QList<DUChainItem>& _items) : items(_items) {
    }
    virtual bool accept(Declaration* decl) {
      if(decl->range().isEmpty())
        return false;
      if(decl->isFunctionDeclaration() || (decl->internalContext() && decl->internalContext()->type() == DUContext::Class)) {

        DUChainItem item;
        item.m_item = IndexedDeclaration(decl);
        item.m_text = decl->toString();
        items << item;

        return true;
      } else
        return false;
    }
    virtual bool accept(DUContext* ctx) {
      if(ctx->type() == DUContext::Class || ctx->type() == DUContext::Namespace || ctx->type() == DUContext::Global || ctx->type() == DUContext::Other || ctx->type() == DUContext::Helper )
        return true;
      else
        return false;
    }
    QList<DUChainItem>& items;
  } filter(items);

  DUChainUtils::collectItems( context, filter );

  if(noHtmlDestriptionInOutline) {
    for(int a = 0; a < items.size(); ++a)
      items[a].m_noHtmlDestription = true;
  }

  Declaration* cursorDecl = cursorContextDeclaration();

  model->registerProvider( QStringList(), QStringList(), new DeclarationListDataProvider(this, items, true) );

  QuickOpenWidgetDialog* dialog = new QuickOpenWidgetDialog( i18n("Outline"), model, QStringList(), QStringList(), true );
  m_currentWidgetHandler = dialog;
  
  model->setParent(dialog->widget());
  
  if(quickOpenLine()) {
    quickOpenLine()->showWithWidget(dialog->widget());
    dialog->deleteLater();
  }else
    dialog->run();

  //Select the declaration that contains the cursor
  if(cursorDecl) {
    int num = 0;
    foreach(const DUChainItem& item, items) {
      if(item.m_item.data() == cursorDecl) {
        dialog->widget()->o.list->setCurrentIndex( model->index(num,0,QModelIndex()) );
        dialog->widget()->o.list->scrollTo( model->index(num,0,QModelIndex()), QAbstractItemView::PositionAtCenter );
      }
      ++num;
    }
  }
}
QuickOpenLineEdit::QuickOpenLineEdit() : m_widget(0), m_forceUpdate(false) {
    setMinimumWidth(200);
    setMaximumWidth(400);
    deactivate();
    setObjectName("Quickopen");
    setFocusPolicy(Qt::ClickFocus);
}
QuickOpenLineEdit::~QuickOpenLineEdit() {
    delete m_widget;
}
void QuickOpenLineEdit::keyPressEvent(QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Escape) {
      kDebug() << "escape";
        deactivate();
        ev->accept();
    }
    QLineEdit::keyPressEvent(ev);
}
bool QuickOpenLineEdit::insideThis(QObject* object) {
    while (object)
    {
        kDebug() << object;
        if (object == this || object == m_widget)
        {
            return true;
        }
        object = object->parent();
    }
    return false;
}

void QuickOpenLineEdit::showWithWidget(QuickOpenWidget* widget)
{
  kDebug() << "storing widget" << widget;
  deactivate();
  if(m_widget) {
    kDebug() << "deleting" << m_widget;
    delete m_widget;
  }
  m_widget = widget;
  m_forceUpdate = true;
  setFocus();
}

void QuickOpenLineEdit::focusInEvent(QFocusEvent* ev) {
    QLineEdit::focusInEvent(ev);
//       delete m_widget;
    kDebug() << "got focus";
    kDebug() << "old widget" << m_widget << "force update:" << m_forceUpdate;
    if (m_widget && !m_forceUpdate)
        return;

    if (!m_forceUpdate && !QuickOpenPlugin::self()->freeModel())
        return;
    
    activate();
    m_forceUpdate = false;
    
    if(!m_widget) {
      m_widget = new QuickOpenWidget( i18n("Quick Open"), QuickOpenPlugin::self()->m_model, QuickOpenPlugin::self()->lastUsedItems, QuickOpenPlugin::self()->lastUsedScopes, false, true );
    }else{
      m_widget->showStandardButtons(false);
      m_widget->showSearchField(false);
    }
    
    m_widget->setParent(0, Qt::ToolTip);
    m_widget->setFocusPolicy(Qt::NoFocus);
    m_widget->setFrameStyle(QFrame::Raised | QFrame::StyledPanel);
    m_widget->setLineWidth(5);
    m_widget->setAlternativeSearchField(this);
    
    QuickOpenPlugin::self()->m_currentWidgetHandler = m_widget;
    connect(m_widget, SIGNAL(ready()), SLOT(deactivate()));

    connect( m_widget, SIGNAL( scopesChanged( const QStringList& ) ), QuickOpenPlugin::self(), SLOT( storeScopes( const QStringList& ) ) );
    Q_ASSERT(m_widget->o.searchLine == this);
    m_widget->prepareShow();
    QRect widgetGeometry = QRect(mapToGlobal(QPoint(0, height())), mapToGlobal(QPoint(width(), height() + 400)));
    widgetGeometry.setWidth(700); ///@todo Waste less space
    QRect screenGeom = QApplication::desktop()->screenGeometry(this);
    if (widgetGeometry.right() > screenGeom.right())
        widgetGeometry.moveRight(screenGeom.right());
    m_widget->setGeometry(widgetGeometry);

    //Hack so we don't have a frame it top, where the widget docks to the line
    QRect frameRect = m_widget->frameRect();
    frameRect.setTop(frameRect.top()-10);
    m_widget->setFrameRect(frameRect);

    m_widget->show();
}

void QuickOpenLineEdit::hideEvent(QHideEvent* ev)
{
    QWidget::hideEvent(ev);
    if(m_widget)
      QMetaObject::invokeMethod(this, "checkFocus", Qt::QueuedConnection);
//       deactivate();
}

bool QuickOpenLineEdit::eventFilter(QObject* obj, QEvent* e) {
    if (!m_widget)
        return false;
    switch (e->type()) {
     case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        kDebug() << "closing because of window activation";
        deactivate();
    break;
    case QEvent::FocusIn:
        if (dynamic_cast<QWidget*>(obj)) {
            QFocusEvent* focusEvent = dynamic_cast<QFocusEvent*>(e);
            Q_ASSERT(focusEvent);
            //Eat the focus event, keep the focus
            kDebug() << "focus change" << "inside this: " << insideThis(obj) << "this" << this << "obj" << obj;
            if(obj == this)
              return false;
            
            kDebug() << "reason" << focusEvent->reason();
            if (focusEvent->reason() != Qt::MouseFocusReason && focusEvent->reason() != Qt::ActiveWindowFocusReason) {
                QMetaObject::invokeMethod(this, "checkFocus", Qt::QueuedConnection);
                return false;
            }
            if (!insideThis(obj))
                deactivate();
        }
        break;
    default:
        break;
    }
    return false;
}
void QuickOpenLineEdit::activate() {
    kDebug() << "activating";
    setText("");
    setStyleSheet("");
    qApp->installEventFilter(this);
}
void QuickOpenLineEdit::deactivate() {
    kDebug() << "deactivating";
    
    setText(i18n("Quick Open"));
    setStyleSheet("color: grey"); ///@todo Better color picking
    if (m_widget) {
        m_widget->deleteLater();
        QMetaObject::invokeMethod(this, "checkFocus", Qt::QueuedConnection);
    }
    m_widget = 0;
    qApp->removeEventFilter(this);
    
}

void QuickOpenLineEdit::checkFocus()
{
    kDebug() << "checking focus" << m_widget;
    if(m_widget) {
      if(isVisible() && !isHidden())
        setFocus();
      else
        deactivate();
    }else{
       if (ICore::self()->documentController()->activeDocument())
           ICore::self()->documentController()->activateDocument(ICore::self()->documentController()->activeDocument());
       
       //Make sure the focus is somewehre else, even if there is no active document
       setEnabled(false);
       setEnabled(true);
    }
}


#include "quickopenplugin.moc"

// kate: space-indent on; indent-width 2; tab-width 4; replace-tabs on; auto-insert-doxygen on
