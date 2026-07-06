import { provideRouter } from '@angular/router';
import { ANSIPipe } from 'src/app/pipes/ansi.pipe';
import { CommonModule } from '@angular/common';
import { SystemApiService } from 'src/app/services/system.service';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { ReactiveFormsModule } from '@angular/forms';
import { ToastService } from 'src/app/services/toast.service';
import { LogsComponent } from './logs.component';
import { ComponentFixture, TestBed } from '@angular/core/testing';

describe('LogsComponent', () => {
  let component: LogsComponent;
  let fixture: ComponentFixture<LogsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [
        CommonModule,
        ReactiveFormsModule,
        ANSIPipe,
        LogsComponent
      ],
      providers: [
        provideRouter([]),
        { provide: ToastService, useValue: { success: jasmine.createSpy(), error: jasmine.createSpy(), warning: jasmine.createSpy() } },
        provideHttpClient(withXhr()),
        SystemApiService
      ]
    })
    .compileComponents();
    
    fixture = TestBed.createComponent(LogsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
