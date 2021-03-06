/*
 * Copyright (C) 2013-2016 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "actionrest_p.h"
#include "context.h"
#include "controller.h"
#include "dispatcher.h"

#include <QUrl>
#include <QDebug>

using namespace Cutelyst;

ActionREST::ActionREST(QObject *parent) : Action(parent)
    , d_ptr(new ActionRESTPrivate)
{
    d_ptr->q_ptr = this;
}

ActionREST::~ActionREST()
{
    delete d_ptr;
}

bool ActionREST::doExecute(Context *c)
{
    Q_D(const ActionREST);

    if (!Action::doExecute(c)) {
        return false;
    }

    return d->dispatchRestMethod(c, c->request()->method());
}

bool ActionRESTPrivate::dispatchRestMethod(Context *c, const QString &httpMethod) const
{
    Q_Q(const ActionREST);
    const QString restMethod = q->name() + QLatin1Char('_') + httpMethod;

    Controller *controller = c->controller();
    Action *action = controller->actionFor(restMethod);
    if (!action) {
        // Look for non registered actions in this controller
        const ActionList actions = controller->actions();
        for (Action *controllerAction : actions) {
            if (controllerAction->name() == restMethod) {
                action = controllerAction;
                break;
            }
        }
    }

    if (action) {
        return c->execute(action);
    }

    bool ret = false;
    if (httpMethod == QLatin1String("OPTIONS")) {
        ret = returnOptions(c, q->name());
    } else if (httpMethod == QLatin1String("HEAD")) {
        // redispatch to GET
        ret = dispatchRestMethod(c, QStringLiteral("GET"));
    } else if (httpMethod != QLatin1String("not_implemented")) {
        // try dispatching to foo_not_implemented
        ret = dispatchRestMethod(c, QStringLiteral("not_implemented"));
    } else {
        // not_implemented
        ret = returnNotImplemented(c, q->name());
    }

    return ret;
}

bool ActionRESTPrivate::returnOptions(Context *c, const QString &methodName) const
{
    Response *response = c->response();
    response->setContentType(QStringLiteral("text/plain"));
    response->setStatus(Response::OK); // 200
    response->setHeader(QStringLiteral("ALLOW"),
                        getAllowedMethods(c->controller(), methodName));
    response->body().clear();
    return true;
}

bool ActionRESTPrivate::returnNotImplemented(Context *c, const QString &methodName) const
{
    Response *response = c->response();
    response->setStatus(Response::MethodNotAllowed); // 405
    response->setHeader(QStringLiteral("ALLOW"),
                        getAllowedMethods(c->controller(), methodName));
    const QString body = QLatin1String("Method ") + c->req()->method()
            + QLatin1String(" not implemented for ") + c->uriFor(methodName).toString();
    response->setBody(body);
    return true;
}

QString Cutelyst::ActionRESTPrivate::getAllowedMethods(Controller *controller, const QString &methodName) const
{
    QStringList methods;
    const QString name = methodName + QLatin1Char('_');
    const ActionList actions = controller->actions();
    for (Action *action : actions) {
        const QString method = action->name();
        if (method.startsWith(name)) {
            methods.append(method.mid(name.size()));
        }
    }

    if (methods.contains(QStringLiteral("GET"))) {
        methods.append(QStringLiteral("HEAD"));
    }

    methods.removeAll(QStringLiteral("not_implemented"));
    methods.sort();
    methods.removeDuplicates();

    return methods.join(QStringLiteral(", "));
}

#include "moc_actionrest.cpp"
