import { TestBed } from '@angular/core/testing';
import { AppComponent } from './app.component';
import { SnowflakesComponent } from './components/snowflakes/snowflakes.component';
import { provideRouter, RouterModule } from '@angular/router';
import { LayoutService } from './layout/service/app.layout.service';
import { ThemeService } from './services/theme.service';
import { LocalStorageService } from './local-storage.service';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { ToastrService } from './services/toast.service';

describe('AppComponent', () => {
  beforeEach(() => TestBed.configureTestingModule({
    imports: [RouterModule, SnowflakesComponent, AppComponent],
    providers: [
      provideRouter([]),
      LayoutService,
      ThemeService,
      LocalStorageService,
      provideHttpClient(withXhr()),
      { provide: ToastrService, useValue: { success: jasmine.createSpy(), error: jasmine.createSpy(), warning: jasmine.createSpy() } }
    ]
  }));

  it('should create the app', () => {
    const fixture = TestBed.createComponent(AppComponent);
    const app = fixture.componentInstance;
    expect(app).toBeTruthy();
  });
});
