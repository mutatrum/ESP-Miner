import { ComponentFixture, TestBed } from '@angular/core/testing';
import { SettingsComponent } from './settings.component';
import { EditComponent } from '../edit/edit.component';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { ToastrService } from 'src/app/services/toast.service';
import { provideRouter } from '@angular/router';

describe('SettingsComponent', () => {
  let component: SettingsComponent;
  let fixture: ComponentFixture<SettingsComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [SettingsComponent, EditComponent],
      providers: [
        provideHttpClient(withXhr()),
        { provide: ToastrService, useValue: { success: jasmine.createSpy(), error: jasmine.createSpy(), warning: jasmine.createSpy() } },
        provideRouter([])
      ]
    });
    fixture = TestBed.createComponent(SettingsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
