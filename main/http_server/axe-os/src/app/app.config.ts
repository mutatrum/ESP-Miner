import { ApplicationConfig, provideZoneChangeDetection } from '@angular/core';
import { provideRouter, withHashLocation } from '@angular/router';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { provideAnimations } from '@angular/platform-browser/animations';
import { LocationStrategy, HashLocationStrategy } from '@angular/common';

import { routes } from './app-routing.module';
import { Api } from './generated/api';
import { ApiConfiguration } from './generated/api-configuration';
import { DialogService } from './services/dialog.service';

export const appConfig: ApplicationConfig = {
  providers: [
    provideZoneChangeDetection({ eventCoalescing: true }),
    provideRouter(routes, withHashLocation()),
    provideHttpClient(withXhr()),
    provideAnimations(),
    { provide: LocationStrategy, useClass: HashLocationStrategy },
    { provide: ApiConfiguration, useValue: { rootUrl: '' } },
    Api,
    DialogService
  ]
};
